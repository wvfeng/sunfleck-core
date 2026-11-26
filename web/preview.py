"""Serve the local Wasm debug build and its inspector connection."""
import argparse
from functools import partial
from hashlib import sha256
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import re
import socket
from threading import Lock
from time import monotonic
from urllib.parse import parse_qs, urlsplit
from urllib.request import Request, urlopen
import webbrowser


ENDPOINT = "/__sunfleck/snapshot"
HEALTH = "/__sunfleck/preview"
UUID = re.compile(r"[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12}")
MAX_SNAPSHOT_BYTES = 16 * 1024 * 1024


class PreviewServer(ThreadingHTTPServer):
    def server_bind(self):
        if hasattr(socket, "SO_EXCLUSIVEADDRUSE"):
            self.allow_reuse_address = False
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        super().server_bind()


class PreviewHandler(SimpleHTTPRequestHandler):
    def inspector_session(self):
        query = parse_qs(urlsplit(self.path).query)
        session = query.get("session", [""])[0]
        return session if UUID.fullmatch(session) else ""

    def inspector_request(self):
        return self.headers.get("X-Sunfleck-Inspector") == "1" and not self.headers.get("Origin")

    def browser_page(self):
        page = self.headers.get("X-Sunfleck-Page", "")
        return page if UUID.fullmatch(page) else None

    def reply(self, payload=b"", content_encoding=None, page=None):
        self.send_response(200 if payload else 204)
        if payload:
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(payload)))
        if content_encoding:
            self.send_header("Content-Encoding", content_encoding)
        if page:
            self.send_header("X-Sunfleck-Page", page)
        self.end_headers()
        if payload:
            self.wfile.write(payload)

    def end_headers(self):
        # A Debug session must load its current JS and Wasm even after a rebuild.
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def do_GET(self):
        path = urlsplit(self.path).path
        if path not in (ENDPOINT, HEALTH):
            super().do_GET()
            return
        if self.headers.get("Host") != self.server.host:
            self.send_error(404)
            return
        if path == HEALTH:
            if self.inspector_request():
                self.reply(self.server.identity)
            else:
                self.send_error(404)
            return
        if self.inspector_request():
            with self.server.lock:
                state = self.server.sessions.get(self.inspector_session())
                snapshot = (state["page"], state["snapshot"]) if state else None
            if snapshot:
                self.reply(snapshot[1], "gzip", snapshot[0])
            else:
                self.reply()
            return
        page = self.browser_page()
        if not page or self.headers.get("Sec-Fetch-Site") != "same-origin":
            self.send_error(404)
            return
        with self.server.lock:
            state = self.server.sessions.get(self.inspector_session())
            now = monotonic()
            controls = {
                "highlight": state["highlight"][0] if now < state["highlight"][1] else "0",
                "pick": state["pick"][0] if now < state["pick"][1] else None,
            } if state and state["page"] == page else {}
        self.reply(json.dumps(controls).encode())

    def do_POST(self):
        if urlsplit(self.path).path != ENDPOINT or self.headers.get("Host") != self.server.host:
            self.send_error(404)
            return
        page = self.browser_page()
        browser = self.headers.get("Origin") == f"http://{self.server.host}"
        editor = self.inspector_request()
        if not page or not (browser or editor):
            self.send_error(404)
            return
        if (self.headers.get_content_type() != "application/json"
                or self.headers.get("Content-Encoding") != ("gzip" if browser else None)):
            self.send_error(415)
            return
        try:
            size = int(self.headers.get("Content-Length", ""))
        except ValueError:
            self.send_error(411)
            return
        if not 0 < size <= (MAX_SNAPSHOT_BYTES if browser else 512):
            self.send_error(413)
            return
        self.connection.settimeout(2)
        try:
            payload = self.rfile.read(size)
        except TimeoutError:
            self.send_error(408)
            return
        if len(payload) != size:
            self.send_error(400)
            return

        session = self.inspector_session()
        if browser:
            with self.server.lock:
                state = self.server.sessions.pop(session, None)
                if not state or state["page"] != page:
                    state = {"page": page, "highlight": ("0", 0), "pick": (None, 0)}
                state["snapshot"] = payload
                self.server.sessions[session] = state
                if len(self.server.sessions) > 8:
                    del self.server.sessions[next(iter(self.server.sessions))]
            self.reply()
            return

        try:
            command = json.loads(payload)
        except (ValueError, UnicodeError):
            self.send_error(400)
            return
        if not isinstance(command, dict):
            self.send_error(400)
            return
        action = command.get("action")
        value = command.get("value")
        now = monotonic()
        valid_highlight = isinstance(value, str) and re.fullmatch(r"[0-9]{1,20}", value)
        valid_pick = isinstance(value, str) and UUID.fullmatch(value)
        with self.server.lock:
            state = self.server.sessions.get(session)
            if not state or state["page"] != page:
                status = 409
            elif action == "highlight" and valid_highlight:
                state["highlight"] = (value, now + 1)
                status = 204
            elif action == "pick" and valid_pick:
                state["pick"] = (value, now + 60)
                status = 204
            elif action == "clear":
                state["highlight"] = ("0", 0)
                state["pick"] = (None, 0)
                status = 204
            else:
                status = 400
        if status == 204:
            self.reply()
        else:
            self.send_error(status)

    def log_request(self, code="-", size="-"):
        if urlsplit(self.path).path not in (ENDPOINT, HEALTH) or str(code) not in ("200", "204"):
            super().log_request(code, size)


def start_server(directory, port, open_inspector=False):
    def show_browser():
        webbrowser.open(f"http://127.0.0.1:{port}/?sunfleckInspector=1")

    identity = json.dumps({"root": sha256(str(Path(directory).resolve()).encode()).hexdigest(),
                           "version": sha256(Path(__file__).read_bytes()).hexdigest()}).encode()
    handler = partial(PreviewHandler, directory=directory)
    try:
        server = PreviewServer(("127.0.0.1", port), handler)
    except OSError as error:
        # Reuse the same workspace's preview after a VS Code window reload.
        request = Request(f"http://127.0.0.1:{port}{HEALTH}", headers={"X-Sunfleck-Inspector": "1"})
        try:
            with urlopen(request, timeout=1) as response:
                compatible = response.read(len(identity) + 1) == identity
        except OSError:
            compatible = False
        if not compatible:
            raise SystemExit(f"Sunfleck preview could not bind port {port}: {error}") from error
        if open_inspector:
            show_browser()
        print(f"Serving HTTP on 127.0.0.1 port {port} (existing preview)", flush=True)
        return
    with server:
        server.host = f"127.0.0.1:{server.server_port}"
        server.identity = identity
        server.sessions = {}
        server.lock = Lock()
        print(f"Serving HTTP on 127.0.0.1 port {server.server_port}", flush=True)
        try:
            if open_inspector:
                show_browser()
            server.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", required=True)
    parser.add_argument("--port", type=int, default=8790)
    parser.add_argument("--open-inspector", action="store_true")
    args = parser.parse_args()
    start_server(args.directory, args.port, args.open_inspector)
