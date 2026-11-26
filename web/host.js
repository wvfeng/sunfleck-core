// Shared by the standalone Emscripten and .NET hosts. All core calls are
// nonblocking. The timer also drains completion work in background tabs.
import { attachTextInput } from './text-input.js';
import { createRenderer } from './renderer.js';

// Explicitly enabled, bounded, local-only traces for native Windows focus/IME
// failures that synthetic browser input does not reproduce. Never record text.
function traceInput(canvas) {
  if (new URL(location.href).searchParams.get('inputDiagnostics') !== '1' || window.sunfleckInputDiagnostics) return;
  const events = [];
  const name = node => node?.hasAttribute?.('data-sunfleck-editor') ? 'editor' : node?.tagName || 'window';
  const snapshot = () => ({ active: name(document.activeElement), hasFocus: document.hasFocus(),
    visibility: document.visibilityState, status: document.querySelector('#status')?.textContent,
    editors: [...document.querySelectorAll('[data-sunfleck-editor]')].map(e => ({ tag: e.tagName,
      disabled: e.disabled, readOnly: e.readOnly, length: e.value.length, first: e.selectionStart, last: e.selectionEnd,
      rect: { x: e.getBoundingClientRect().x, y: e.getBoundingClientRect().y, width: e.offsetWidth, height: e.offsetHeight } })) });
  window.sunfleckInputDiagnostics = { events, snapshot, report: () => ({ snapshot: snapshot(), events: [...events] }) };
  for (const type of ['pointerdown', 'pointerup', 'click', 'focus', 'blur', 'keydown', 'keyup', 'beforeinput', 'input', 'compositionstart', 'compositionend']) {
    window.addEventListener(type, event => {
      if (event.target !== window && event.target !== canvas && !event.target?.hasAttribute?.('data-sunfleck-editor')) return;
      const entry = { type, time: Math.round(performance.now()), target: name(event.target), trusted: event.isTrusted,
        key: event.key?.length === 1 ? '[character]' : event.key, keyCode: event.keyCode,
        composing: event.isComposing, inputType: event.inputType, before: snapshot() };
      events.push(entry); if (events.length > 200) events.shift();
      queueMicrotask(() => { entry.after = snapshot(); entry.prevented = event.defaultPrevented; });
    }, true);
  }
}

export async function loadHostedFont(source) {
  let objectUrl = null;
  if (typeof source !== 'string') {
    objectUrl = URL.createObjectURL(new Blob([source], { type: 'font/ttf' }));
  }
  const sourceUrl = objectUrl || new URL(source, document.baseURI).href;
  const face = new FontFace('SunfleckFont', `url("${sourceUrl}")`, {
    style: 'normal', weight: '400', stretch: 'normal',
  });
  try {
    await face.load();
    document.fonts.add(face);
  } finally {
    if (objectUrl) URL.revokeObjectURL(objectUrl);
  }
}

function wasmInspector(module) {
  if (!module?._sunfleck_web_debug_snapshot) return;
  return {
    snapshot: () => {
      const pointer = module._sunfleck_web_debug_snapshot();
      return pointer ? module.UTF8ToString(pointer) : '';
    },
    highlight: id => {
      const object = BigInt(id);
      return module._sunfleck_web_debug_highlight(Number(object & 0xffffffffn), Number(object >> 32n));
    },
    pick: enabled => module._sunfleck_web_debug_pick(Number(enabled)),
    pointer: (view, action, x, y) => module._sunfleck_web_debug_pointer(view, action, x, y),
  };
}

export function attachHost(canvas, backend, viewId = 0, rendererMode = new URL(location.href).searchParams.get('renderer') || 'auto') {
  traceInput(canvas);
  let lastFrame = -1, layoutChanged = true, lastDpi = 0;
  backend.frameVersion ??= 0;
  const runtime = new Proxy(backend, { get(target, name) {
    if (typeof target[name] !== 'function') return target[name];
    return (...args) => {
      if (name !== 'snapshot' && name !== 'stop' && name !== 'tick') {
        const result = target.selectView(viewId);
        if (result !== 0) throw new Error(`View unavailable: ${viewId}`);
      }
      if (name === 'tick' && viewId) {
        if (lastFrame === backend.frameVersion) return 7;
        lastFrame = backend.frameVersion; return 0;
      }
      if (name === 'stop' && viewId) return;
      const result = target[name](...args);
      if (name === 'tick' && result === 0) ++backend.frameVersion;
      return result;
    };
  } });
  const inspector = viewId ? undefined : backend.inspector ?? wasmInspector(backend.wasmModule);
  let closeInspector;
  if (inspector && location.protocol === 'http:' && location.hostname === '127.0.0.1' &&
      new URL(location.href).searchParams.get('sunfleckInspector') === '1') {
    backend.inspectorSurfaces = new Map();
    void import('./inspector.js').then(({ startInspector }) => {
      if (!closed) closeInspector = startInspector(new URL(location.href), inspector, backend.inspectorSurfaces);
    }).catch(error => console.error('Sunfleck Inspector', error));
  }
  if (backend.inspectorSurfaces) {
    backend.inspectorSurfaces.set(canvas, viewId);
    canvas.style.cursor = 'var(--sunfleck-pick-cursor, auto)';
  }
  const children = new Map();
  const semanticNodes = new Map();
  const semanticGroups = new Map();
  let syncingSemanticFocus = false;
  const semantics = document.createElement('div');
  semantics.setAttribute('role', 'group'); semantics.setAttribute('aria-label', viewId ? 'Floating panel' : 'Application controls');
  Object.assign(semantics.style, { position: 'fixed', inset: '0', pointerEvents: 'none', color: 'transparent' });
  canvas.after(semantics);
  function syncSemantics(nodes) {
    const bounds = canvas.getBoundingClientRect();
    const sx = bounds.width / canvas.width, sy = bounds.height / canvas.height;
    const live = new Set();
    let focusedElement = null;
    for (const node of nodes) {
      live.add(node.id);
      let element = semanticNodes.get(node.id);
      if (!element) {
        const tag = node.role === 'status' ? 'span' :
          ['treeitem', 'slider', 'option', 'article'].includes(node.role) ? 'div' : 'button';
        element = document.createElement(tag);
        if (tag === 'button') element.type = 'button';
        if (tag !== 'span' && node.role !== 'article') element.tabIndex = -1;
        element.dataset.nodeId = node.id;
        element.addEventListener('focus', () => {
          if (syncingSemanticFocus || element.disabled) return;
          check(runtime.semanticAction(node.id, 0));
          if (node.role === 'textbox') {
            canvas.focus({ preventScroll: true });
            text.sync(true);
          }
        });
        element.addEventListener('click', event => {
          event.stopPropagation();
          if (node.role === 'status') return;
          if (node.role === 'textbox') {
            canvas.focus({ preventScroll: true });
            check(runtime.semanticAction(node.id, 0));
          } else {
            check(runtime.semanticAction(node.id, 1));
            focusSemantic(element);
          }
          text.sync(true);
        });
        semantics.append(element); semanticNodes.set(node.id, element);
      }
      const groupRole = { option: 'listbox', tab: 'tablist', treeitem: 'tree' }[node.role];
      if (groupRole) {
        const key = `${groupRole}:${node.parent}`;
        let group = semanticGroups.get(key);
        if (!group) {
          group = document.createElement('div'); group.setAttribute('role', groupRole);
          group.setAttribute('aria-label', groupRole === 'tree' ? 'Tree' : groupRole === 'listbox' ? 'List' : 'Tabs');
          semantics.append(group); semanticGroups.set(key, group);
        }
        const parent = semanticNodes.get(node.parent);
        if (node.role === 'treeitem' && parent?.getAttribute('role') === 'treeitem') { group.setAttribute('role', 'group'); parent.append(group); }
        if (element.parentElement !== group) group.append(element);
      }
      element.setAttribute('role', node.role); element.setAttribute('aria-label', node.name);
      Object.assign(element.style, { position: 'fixed', left: `${bounds.left + node.x * sx}px`, top: `${bounds.top + node.y * sy}px`,
        width: `${node.width * sx}px`, height: `${node.height * sy}px`, margin: '0', padding: '0', border: '0',
        pointerEvents: 'none', appearance: 'none', background: 'transparent', color: 'transparent', outline: 'none' });
      if (node.role === 'status' && element.textContent !== node.value) element.textContent = node.value;
      element.disabled = !node.enabled;
      element.setAttribute('aria-disabled', String(!node.enabled));
      element.setAttribute('aria-hidden', String(node.role === 'textbox' && node.focused && text.owns(document.activeElement)));
      if (node.focused && node.enabled) {
        if (node.role === 'textbox') text.setLabel(node.name);
        else focusedElement = element;
      }
      if (node.role === 'checkbox' || node.role === 'radio') element.setAttribute('aria-checked', String(node.checked));
      if (['option', 'tab', 'treeitem'].includes(node.role)) element.setAttribute('aria-selected', String(node.selected));
      if (node.role === 'treeitem') element.setAttribute('aria-expanded', String(node.expanded));
      if (node.role === 'slider') {
        element.setAttribute('aria-valuemin', '0'); element.setAttribute('aria-valuemax', '100'); element.setAttribute('aria-valuenow', String(node.number));
      }
      if (node.role === 'textbox') element.setAttribute('aria-valuetext', node.value);
    }
    for (const [id, element] of semanticNodes) if (!live.has(id)) { element.remove(); semanticNodes.delete(id); }
    for (const [id, element] of semanticGroups) if (!element.querySelector('[data-node-id]')) { element.remove(); semanticGroups.delete(id); }
    const active = document.activeElement;
    if (focusedElement && pointer === null && (active === canvas || semantics.contains(active))) {
      focusSemantic(focusedElement);
    }
  }
  function focusSemantic(element) {
    if (document.activeElement === element) return;
    syncingSemanticFocus = true;
    try { element.focus({ preventScroll: true }); }
    finally { syncingSemanticFocus = false; }
  }
  function syncViews(snapshot) {
    const bounds = canvas.getBoundingClientRect();
    const live = new Set();
    for (const view of snapshot.views) {
      if (!view.id) { syncSemantics(view.nodes); continue; }
      live.add(view.id);
      let child = children.get(view.id);
      if (!child) {
        const surface = document.createElement('canvas'); surface.tabIndex = 0; surface.dataset.viewId = view.id;
        surface.setAttribute('aria-label', 'Floating panel');
        Object.assign(surface.style, { position: 'fixed', borderRadius: '0', border: '0', boxShadow: '0 4px 18px #0004' });
        document.body.append(surface);
        child = { canvas: surface, host: attachHost(surface, backend, view.id, renderer.kind === 'canvas2d' ? 'canvas2d' : rendererMode) }; children.set(view.id, child);
      }
      const logical = 96 / view.dpi;
      Object.assign(child.canvas.style, { left: `${bounds.left + view.x * logical}px`, top: `${bounds.top + view.y * logical}px`,
        width: `${view.width * logical}px`, height: `${view.height * logical}px`, zIndex: String(10 + snapshot.views.indexOf(view) * 3) });
      child.host.syncSemantics(view.nodes);
    }
    for (const [id, child] of children) if (!live.has(id)) { child.host.close(); child.canvas.remove(); children.delete(id); }
  }
  if (!viewId) backend.refreshViews = () => syncViews(backend.snapshot());
  const renderer = createRenderer(canvas, rendererMode, fail);
  let closed = false, suspended = false, pointer = null, frame = 0, wheelRemainder = 0;
  const events = new AbortController();
  const options = { signal: events.signal };
  const check = value => { if (value !== 0) throw new Error(`Sunfleck error: ${value}`); };
  if (!viewId) check(backend.setRenderMode(renderer.kind === 'webgl2' ? 1 : 0));
  const text = attachTextInput(canvas, runtime, fail);
  function fail(error) {
    close(); document.querySelector('#status').textContent = String(error); console.error(error);
  }
  function resize() {
    const ratio = Math.min(4, Math.max(0.5, window.devicePixelRatio || 1));
    const dpi = Math.round(96 * ratio);
    const deferView = Boolean(viewId && dpi !== lastDpi);
    if (lastDpi && dpi !== lastDpi) {
      cancel();
      // Cancellation may restore core geometry. Refresh the DOM before measuring
      // it, otherwise the abandoned resize is written back at the new DPI.
      if (viewId) backend.refreshViews();
    }
    const bounds = canvas.getBoundingClientRect();
    const w = Math.max(1, Math.round(bounds.width * ratio));
    const h = Math.max(1, Math.round(bounds.height * ratio));
    if (!viewId || dpi !== lastDpi) { check(runtime.resize(w, h, dpi)); lastDpi = dpi; }
    if (canvas.width !== w || canvas.height !== h) { canvas.width = w; canvas.height = h; }
    return deferView;
  }
  function paint() {
    if (closed || suspended) return;
    try {
      // Child DPI changes happen after the main Poll. Wait for the next shared
      // Poll before reading that child's commands at its new dimensions.
      if (resize() && viewId) return;
      const result = runtime.tick();
      if (result === 8) { close(); return; }
      if (result !== 0 && result !== 7) throw new Error(`Frame failed: ${result}`);
      if (pointer === null) text.sync();
      if (result === 0) {
        renderer.present(renderer.kind === 'webgl2' ? runtime.commands() : runtime.pixels(canvas.width, canvas.height));
      }
      if (!viewId && (result === 0 || layoutChanged)) { syncViews(runtime.snapshot()); layoutChanged = false; }
      if (!viewId) for (const child of children.values()) child.host.paint();
    } catch (error) {
      fail(error);
    }
  }
  function tick() { paint(); if (!closed && !suspended && !viewId) frame = requestAnimationFrame(tick); }
  function position(event) {
    const bounds = canvas.getBoundingClientRect();
    return [Math.round((event.clientX - bounds.left) * canvas.width / bounds.width),
      Math.round((event.clientY - bounds.top) * canvas.height / bounds.height)];
  }
  canvas.addEventListener('pointerdown', event => {
    if (event.button !== 0 || pointer !== null) return;
    canvas.focus({ preventScroll: true }); pointer = event.pointerId; canvas.setPointerCapture(pointer);
    check(runtime.pointer(0, ...position(event))); event.preventDefault();
  }, options);
  canvas.addEventListener('pointermove', event => {
    if (pointer === null || pointer === event.pointerId) check(runtime.pointer(1, ...position(event)));
  }, options);
  canvas.addEventListener('pointerup', event => {
    if (pointer !== event.pointerId) return;
    check(runtime.pointer(2, ...position(event)));
    const id = pointer; pointer = null;
    if (canvas.hasPointerCapture(id)) canvas.releasePointerCapture(id);
    if (viewId) { backend.refreshViews(); if (closed) return; }
    text.sync(true);
  }, options);
  function cancel() {
    if (pointer !== null) {
      const id = pointer; pointer = null; check(runtime.pointer(3, 0, 0));
      if (canvas.hasPointerCapture(id)) canvas.releasePointerCapture(id);
    }
  }
  canvas.addEventListener('pointercancel', cancel, options);
  function dropEditor(event) {
    if (suspended || !event.dataTransfer?.types.includes('text/plain') ||
        event.dataTransfer.types.includes('Files')) return null;
    for (const element of [...semanticNodes.values()].reverse()) {
      if (element.disabled || element.getAttribute('role') !== 'textbox') continue;
      const bounds = element.getBoundingClientRect();
      if (event.clientX >= bounds.left && event.clientX < bounds.right &&
          event.clientY >= bounds.top && event.clientY < bounds.bottom) return element;
    }
    return null;
  }
  canvas.addEventListener('dragover', event => {
    if (!dropEditor(event)) return;
    event.preventDefault();
    event.dataTransfer.dropEffect = 'copy';
  }, options);
  canvas.addEventListener('drop', event => {
    const target = dropEditor(event);
    if (!target) return;
    const value = event.dataTransfer.getData('text/plain');
    if (!value) return;
    event.preventDefault();
    canvas.focus({ preventScroll: true });
    check(runtime.semanticAction(target.dataset.nodeId, 0));
    text.sync(true);
    text.insertDrop(event, value);
  }, options);
  if (backend.fileDrop) {
    const dropTarget = viewId === 0 ? backend.fileDropTarget : null;
    const isDedicatedTarget = event => dropTarget?.contains(event.target);
    const acceptsFiles = event => !suspended && event.dataTransfer?.types.includes('Files') &&
      (event.target === canvas || text.owns(event.target) || isDedicatedTarget(event));
    for (const type of ['dragenter', 'dragover']) {
      document.addEventListener(type, event => {
        if (!acceptsFiles(event)) return;
        event.preventDefault();
        event.dataTransfer.dropEffect = 'copy';
      }, options);
    }
    document.addEventListener('drop', event => {
      if (!acceptsFiles(event)) return;
      event.preventDefault();
      const files = Array.from(event.dataTransfer.files);
      if (!files.length) return;
      const [x, y] = isDedicatedTarget(event) ? [null, null] : position(event);
      backend.fileDrop({ files, viewId, x, y });
    }, options);
  }
  canvas.addEventListener('contextmenu', event => { check(runtime.contextMenu(...position(event))); event.preventDefault(); }, options);
  canvas.addEventListener('wheel', event => {
    const scale = canvas.height / canvas.getBoundingClientRect().height;
    const units = event.deltaMode === 1 ? 16 : event.deltaMode === 2 ? canvas.height / scale : 1;
    wheelRemainder += event.deltaY * units * scale;
    const dy = Math.max(-32768, Math.min(32768, Math.trunc(wheelRemainder)));
    wheelRemainder -= dy;
    check(runtime.scroll(...position(event), dy)); text.sync(); event.preventDefault();
  }, { ...options, passive: false });
  canvas.addEventListener('lostpointercapture', cancel, options);
  function leaveView(event) {
    if (event.relatedTarget === canvas || semantics.contains(event.relatedTarget) || text.owns(event.relatedTarget)) return;
    check(runtime.focus(0));
  }
  canvas.addEventListener('blur', event => { cancel(); leaveView(event); }, options);
  semantics.addEventListener('focusout', leaveView, options);
  canvas.addEventListener('focus', () => check(runtime.focus(1)), options);
  function handleKey(event) {
    const keys = { Tab: 0, Enter: 1, ' ': 1, ArrowLeft: 5, ArrowUp: 7, ArrowRight: 6, ArrowDown: 8, Escape: 4 };
    if (event.key in keys) {
      if (event.key === 'Escape') cancel();
      check(runtime.key(keys[event.key], Number(event.shiftKey)));
      event.preventDefault();
      if (runtime.readText?.() && document.activeElement !== canvas) canvas.focus({ preventScroll: true });
      text.sync(true);
    }
  }
  canvas.addEventListener('keydown', handleKey, options);
  semantics.addEventListener('keydown', handleKey, options);
  function close() {
    if (closed) return;
    closed = true; events.abort(); clearInterval(timer); cancelAnimationFrame(frame);
    closeInspector?.();
    backend.inspectorSurfaces?.delete(canvas);
    for (const child of children.values()) { child.host.close(); child.canvas.remove(); }
    children.clear(); semantics.remove(); text.close(); renderer.close(); runtime.stop();
    if (!viewId) document.querySelector('#status').textContent = 'Closed';
  }
  window.addEventListener('blur', cancel, options);
  window.addEventListener('scroll', () => { layoutChanged = true; }, { ...options, capture: true });
  window.addEventListener('resize', () => { layoutChanged = true; }, options);
  window.addEventListener('pagehide', event => {
    if (!event.persisted) { close(); return; }
    cancel(); suspended = true; cancelAnimationFrame(frame);
  }, options);
  window.addEventListener('pageshow', event => {
    if (event.persisted && suspended && !closed) { suspended = false; if (!viewId) frame = requestAnimationFrame(tick); }
  }, options);
  const timer = viewId ? 0 : setInterval(() => { if (document.hidden) paint(); }, 100);
  if (!viewId) frame = requestAnimationFrame(tick);
  return { close, syncSemantics, paint };
}
