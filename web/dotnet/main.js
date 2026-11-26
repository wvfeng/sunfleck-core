import { dotnet } from './_framework/dotnet.js';
import { attachHost, loadHostedFont } from './host.js';

try {
  const runtime = await dotnet.create();
  const exports = await runtime.getAssemblyExports(runtime.getConfig().mainAssemblyName);
  const app = exports.BrowserApp;
  // Both renderers must receive the same font, including after rebuilding a
  // local preview at the same URL with a different font file.
  const response = await fetch('./font.ttf', { cache: 'no-cache' });
  if (!response.ok) throw new Error(`Font request failed: ${response.status}`);
  const font = new Uint8Array(await response.arrayBuffer());
  await loadHostedFont(font);
  let page = '';
  if (new URL(location.href).searchParams.get('scenario') === 'workspace') {
    const response = await fetch('./workspace.fleck');
    if (!response.ok) throw new Error(`Page request failed: ${response.status}`);
    page = await response.text();
  }
  app.Start(font, page, new URL('.', location.href).href);
  if (new URL(location.href).searchParams.has('verify')) { app.VerifyContracts(); document.body.dataset.contracts = 'passed'; }
  const host = attachHost(document.querySelector('canvas'), {
    inspector: app.DebugSnapshot && {
      snapshot: app.DebugSnapshot, highlight: app.DebugHighlight, pick: app.DebugPick, pointer: app.DebugPointer,
    },
    setRenderMode: app.SetRenderMode, commands: app.DrawCommands,
    selectView: app.SelectView, snapshot: () => JSON.parse(app.Snapshot()), semanticAction: app.SemanticAction,
    resize: app.Resize, pointer: app.Pointer, key: app.Key, focus: app.Focus,
    scroll: app.Scroll,
    contextMenu: app.ContextMenu,
    readText: () => JSON.parse(app.ReadText()), updateText: app.UpdateText,
    previewText: (session, revision, text, first, last, backward, compositionFirst, compositionLast) =>
      app.PreviewText(session, revision, JSON.stringify({ text, first, last, backward: Boolean(backward), compositionFirst, compositionLast })),
    hitText: app.HitText, scrollText: app.ScrollText,
    fileDropTarget: document.querySelector('#file-drop-target'),
    fileDrop: ({ files }) => {
      document.querySelector('#file-drop-status').textContent =
        files.map(file => `${file.name} (${file.size} B)`).join(', ');
    },
    tick: app.Tick, pixels: () => app.Pixels(), stop: app.Stop,
  });
  document.querySelector('#stop').onclick = () => host.close();
  document.querySelector('#status').textContent = 'Ready';
} catch (error) {
  document.querySelector('#status').textContent = String(error);
  console.error(error);
}
