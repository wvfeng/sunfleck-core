import createModule from './sunfleck.js';
import { attachHost, loadHostedFont } from './host.js';

try {
  // Emscripten's generated preload XHR may leave module readiness pending on
  // HTTP failures. Fetch explicitly so deployment errors reach the status UI.
  const packageResponse = await fetch('./sunfleck.data', { cache: 'no-cache' });
  if (!packageResponse.ok) throw new Error(`Font package request failed: ${packageResponse.status}`);
  let packageBytes = await packageResponse.arrayBuffer();
  const module = await createModule({ getPreloadedPackage: () => {
    const bytes = packageBytes; packageBytes = null; return bytes;
  } });
  await loadHostedFont(module.FS.readFile('/sunfleck/font.ttf'));
  function withText(text, callback) {
    const bytes = new TextEncoder().encode(text);
    const pointer = module._sf_alloc(bytes.length + 1);
    if (!pointer) throw new Error('Text allocation failed');
    try { module.HEAPU8.set(bytes, pointer); module.HEAPU8[pointer + bytes.length] = 0; return callback(pointer, bytes.length); }
    finally { module._sf_free(pointer); }
  }
  const scenario = new URL(location.href).searchParams.get('scenario');
  let page = '';
  if (scenario === 'workspace') {
    const response = await fetch('./workspace.fleck');
    if (!response.ok) throw new Error(`Page request failed: ${response.status}`);
    page = await response.text();
  }
  const status = withText(page, pointer => module._sf_start(pointer));
  if (status !== 0) throw new Error(`Start failed: ${status}`);
  if (new URL(location.href).searchParams.has('verify')) {
    if (module._sf_verify_contracts() !== 0) throw new Error('Hosted contract verification failed');
    document.body.dataset.contracts = 'passed';
  }
  const host = attachHost(document.querySelector('canvas'), {
    setRenderMode: mode => module._sf_render_mode(mode),
    commands: () => {
      const sizePointer = module._sf_alloc(4);
      if (!sizePointer) throw new Error('Command allocation failed');
      try {
        const pointer = module._sf_draw_commands(sizePointer);
        const size = new DataView(module.HEAPU8.buffer).getUint32(sizePointer, true);
        if (!pointer || !size) throw new Error('Draw commands unavailable');
        return module.HEAPU8.subarray(pointer, pointer + size);
      } finally { module._sf_free(sizePointer); }
    },
    selectView: id => module._sf_select_view(id),
    snapshot: () => JSON.parse(module.UTF8ToString(module._sf_snapshot())),
    semanticAction: (id, action) => withText(id, pointer => module._sf_semantic_action(pointer, action)),
    resize: (w, h, dpi) => module._sf_resize(w, h, dpi),
    pointer: (kind, x, y) => module._sf_pointer(kind, x, y),
    key: (kind, shift) => module._sf_key(kind, shift),
    focus: value => module._sf_focus(value),
    scroll: (x, y, dy) => module._sf_scroll(x, y, dy),
    contextMenu: (x, y) => module._sf_context_menu(x, y),
    readText: () => {
      const state = JSON.parse(module.UTF8ToString(module._sf_text_read()));
      if (state?.error) throw new Error(`Text read failed: ${state.error}`);
      return state;
    },
    updateText: (session, revision, text, first, last, backward) =>
      withText(text, (pointer, size) => module._sf_text_update(session, revision, pointer, size, first, last, backward)),
    previewText: (session, revision, text, first, last, backward, cs, ce) =>
      withText(text, (pointer, size) => module._sf_text_preview(session, revision, pointer, size, first, last, backward, cs, ce)),
    hitText: (session, revision, x, y) => module._sf_text_hit(session, revision, x, y),
    scrollText: (session, revision, dx, dy) => module._sf_text_scroll(session, revision, dx, dy),
    fileDropTarget: document.querySelector('#file-drop-target'),
    fileDrop: ({ files }) => {
      document.querySelector('#file-drop-status').textContent =
        files.map(file => `${file.name} (${file.size} B)`).join(', ');
    },
    tick: () => module._sf_tick(),
    pixels: (w, h) => {
      // Reacquire after every native call: memory growth invalidates old views.
      const pointer = module._sf_pixels();
      return module.HEAPU8.subarray(pointer, pointer + w * h * 4);
    },
    stop: () => module._sf_stop(),
  });
  document.querySelector('#stop').onclick = () => host.close();
  document.querySelector('#status').textContent = 'Ready';
} catch (error) {
  document.querySelector('#status').textContent = String(error);
  console.error(error);
}
