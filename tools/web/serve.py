"""Local deployment verifier: explicit MIME, revalidation, gzip sidecars and no directory listings."""
import argparse
import functools
import gzip
import hashlib
import http.server
import json
from pathlib import Path

class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {**http.server.SimpleHTTPRequestHandler.extensions_map,
                      '.wasm': 'application/wasm', '.js': 'text/javascript', '.fleck': 'application/xml',
                      '.ttf': 'font/ttf', '.json': 'application/json'}

    def end_headers(self):
        self.send_header('Cache-Control', 'no-cache')
        self.send_header('X-Content-Type-Options', 'nosniff')
        super().end_headers()

    def list_directory(self, path):
        self.send_error(403)
        return None

    def do_GET(self):
        file = Path(self.translate_path(self.path))
        compressed = Path(str(file) + '.gz')
        if file.is_file() and compressed.is_file() and 'gzip' in self.headers.get('Accept-Encoding', ''):
            self.send_response(200)
            self.send_header('Content-Type', self.guess_type(str(file)))
            self.send_header('Content-Encoding', 'gzip')
            self.send_header('Vary', 'Accept-Encoding')
            self.send_header('Content-Length', str(compressed.stat().st_size))
            self.end_headers()
            self.wfile.write(compressed.read_bytes())
        else:
            super().do_GET()

def package(root):
    assets = []
    for file in sorted(root.rglob('*')):
        if not file.is_file() or file.suffix in {'.gz', '.br'} or file.name == 'asset-manifest.json':
            continue
        data = file.read_bytes()
        assets.append({'path': file.relative_to(root).as_posix(), 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
        if file.suffix in {'.js', '.wasm', '.data', '.dat', '.ttf', '.fleck', '.json', '.html'}:
            Path(str(file) + '.gz').write_bytes(gzip.compress(data, compresslevel=6, mtime=0))
    (root / 'asset-manifest.json').write_text(json.dumps({'version': 1, 'assets': assets}, indent=2), encoding='utf-8')

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('directory', type=Path)
    parser.add_argument('--port', type=int, default=8768)
    parser.add_argument('--package', action='store_true')
    args = parser.parse_args()
    root = args.directory.resolve(strict=True)
    if args.package:
        package(root)
    else:
        http.server.ThreadingHTTPServer(('127.0.0.1', args.port), functools.partial(Handler, directory=str(root))).serve_forever()
