// A transparent native editor owns browser editing/IME/undo. All visible text,
// selection, composition and caret pixels come from the core renderer.
export function attachTextInput(canvas, runtime, onError) {
  let editor = null, state = null, composing = false, syncing = false, disposed = false;
  let editorEvents = null, hadPreview = false, compositionBase = null, drag = null;
  const events = new AbortController();
  const owns = target => target === editor && editor !== null;
  const guarded = callback => (...args) => { try { callback(...args); } catch (error) { onError(error); } };
  function remove() {
    editorEvents?.abort(); editorEvents = null;
    syncing = true;
    editor?.remove(); editor = null; state = null; composing = false; hadPreview = false;
    compositionBase = null; drag = null;
    syncing = false;
  }
  function selection() {
    let first = editor.selectionStart ?? 0, last = editor.selectionEnd ?? first;
    const insidePair = index => index > 0 && index < editor.value.length &&
      editor.value.charCodeAt(index - 1) >= 0xd800 && editor.value.charCodeAt(index - 1) <= 0xdbff &&
      editor.value.charCodeAt(index) >= 0xdc00 && editor.value.charCodeAt(index) <= 0xdfff;
    if (first === last && insidePair(first)) { ++first; ++last; }
    else { if (insidePair(first)) --first; if (insidePair(last)) ++last; }
    return [first, last, Number(editor.selectionDirection === 'backward')];
  }
  function reconcile(result) {
    if (result !== 0 && result !== 6) throw new Error(`Text update failed: ${result}`);
    sync(result === 6);
  }
  function commit() {
    if (!editor || !state || syncing || disposed) return;
    if (composing) { preview(); return; }
    const [first, last, backward] = selection();
    if (!hadPreview && editor.value === state.text && first === state.selectionStart && last === state.selectionEnd && backward === state.selectionBackward) return;
    const result = runtime.updateText(state.session, state.revision, editor.value, first, last, backward);
    hadPreview = false;
    reconcile(result);
  }
  function preview() {
    if (!editor || !state || syncing || !composing || disposed) return;
    const [first, last, backward] = selection();
    const base = compositionBase;
    let start = base.first, end = editor.value.length - (base.text.length - base.last);
    // Some IMEs revise surrounding text rather than only the initial selection.
    if (end < start || !editor.value.startsWith(base.text.slice(0, base.first)) ||
        !editor.value.endsWith(base.text.slice(base.last))) {
      start = 0; end = editor.value.length;
    }
    const result = runtime.previewText(state.session, state.revision, editor.value, first, last, backward, start, end);
    hadPreview = result === 0;
    reconcile(result);
  }
  function point(event) {
    const bounds = canvas.getBoundingClientRect();
    return [Math.max(-32768, Math.min(32768, Math.round((event.clientX - bounds.left) * canvas.width / bounds.width))),
      Math.max(-32768, Math.min(32768, Math.round((event.clientY - bounds.top) * canvas.height / bounds.height)))];
  }
  function hit(event) {
    const offset = runtime.hitText(state.session, state.revision, ...point(event));
    if (offset < 0) { reconcile(-offset); return null; }
    return offset;
  }
  function select(first, last) {
    editor.setSelectionRange(Math.min(first, last), Math.max(first, last), first > last ? 'backward' : 'forward');
    commit();
  }
  const insertDrop = guarded((event, value) => {
    if (!editor || !state || composing || disposed) return;
    const offset = hit(event);
    if (offset === null) return;
    if (!state.multiline) value = value.replace(/[\r\n]/g, '');
    let end = editor.maxLength - editor.value.length;
    if (end < value.length && end > 0 &&
        value.charCodeAt(end - 1) >= 0xd800 && value.charCodeAt(end - 1) <= 0xdbff) --end;
    if (!value || end <= 0) return;
    editor.setRangeText(value.slice(0, end), offset, offset, 'end');
    commit();
  });
  function create(snapshot) {
    remove();
    editor = document.createElement(snapshot.multiline ? 'textarea' : 'input');
    if (!snapshot.multiline) editor.type = snapshot.password ? 'password' : 'text';
    editor.dataset.sunfleckEditor = '';
    editor.setAttribute('aria-label', snapshot.password ? 'Password' : snapshot.multiline ? 'Multiline text' : 'Text');
    editor.autocomplete = 'off'; editor.spellcheck = false; editor.maxLength = 262144;
    Object.assign(editor.style, { position: 'fixed', boxSizing: 'border-box', margin: '0',
      border: '1px solid transparent', borderRadius: '3px', outline: 'none', resize: 'none',
      opacity: '0', caretColor: 'transparent', cursor: 'text', touchAction: 'none',
      fontFamily: '"SunfleckFont", system-ui, sans-serif', fontWeight: '400',
      fontStyle: 'normal', letterSpacing: '0', lineHeight: 'normal',
      wordBreak: 'break-all', overflowWrap: 'anywhere', zIndex: '10' });
    editorEvents = new AbortController();
    const options = { signal: editorEvents.signal };
    editor.addEventListener('compositionstart', () => {
      composing = true;
      const [first, last] = selection();
      compositionBase = { text: editor.value, first, last };
    }, options);
    editor.addEventListener('compositionupdate', guarded(() => {
      // The composition event precedes the DOM value change on Chromium.
      queueMicrotask(guarded(() => { if (composing) preview(); }));
    }), options);
    editor.addEventListener('compositionend', guarded(() => { composing = false; commit(); compositionBase = null; }), options);
    editor.addEventListener('input', guarded(() => commit()), options);
    editor.addEventListener('select', guarded(() => commit()), options);
    editor.addEventListener('pointerdown', guarded(event => {
      if (event.button !== 0 || composing) return;
      const offset = hit(event); if (offset === null) return;
      event.preventDefault(); editor.focus({ preventScroll: true });
      const anchor = event.shiftKey ? (editor.selectionDirection === 'backward' ? editor.selectionEnd : editor.selectionStart) : offset;
      drag = { id: event.pointerId, anchor };
      editor.setPointerCapture(event.pointerId); select(anchor, offset);
    }), options);
    editor.addEventListener('dblclick', guarded(event => {
      if (composing) return;
      const offset = hit(event); if (offset === null) return;
      const parts = new Intl.Segmenter(undefined, { granularity: 'word' }).segment(editor.value);
      const part = parts.containing(offset === editor.value.length ? Math.max(0, offset - 1) : offset);
      if (part) { event.preventDefault(); select(part.index, part.index + part.segment.length); }
    }), options);
    editor.addEventListener('click', guarded(event => {
      if (event.detail < 3 || composing) return;
      const offset = hit(event); if (offset === null) return;
      const start = offset === 0 ? 0 : editor.value.lastIndexOf('\n', offset - 1) + 1;
      const next = editor.value.indexOf('\n', offset);
      event.preventDefault(); select(start, next < 0 ? editor.value.length : next + 1);
    }), options);
    editor.addEventListener('pointermove', guarded(event => {
      if (!drag || drag.id !== event.pointerId || composing) return;
      const offset = hit(event); if (offset !== null) select(drag.anchor, offset);
      event.preventDefault();
    }), options);
    const endDrag = event => {
      if (drag?.id !== event.pointerId) return;
      drag = null; if (editor.hasPointerCapture(event.pointerId)) editor.releasePointerCapture(event.pointerId);
    };
    editor.addEventListener('pointerup', endDrag, options);
    editor.addEventListener('pointercancel', endDrag, options);
    editor.addEventListener('lostpointercapture', () => { drag = null; }, options);
    editor.addEventListener('wheel', guarded(event => {
      if (event.ctrlKey || !state) return;
      const scale = event.deltaMode === 1 ? state.lineHeight : event.deltaMode === 2 ? state.bottom - state.top : canvas.height / canvas.getBoundingClientRect().height;
      const delta = value => Math.max(-32768, Math.min(32768, Math.round(value * scale)));
      reconcile(runtime.scrollText(state.session, state.revision, delta(event.deltaX || (!state.multiline ? event.deltaY : 0)), state.multiline ? delta(event.deltaY) : 0));
      event.preventDefault();
    }), { ...options, passive: false });
    editor.addEventListener('keydown', guarded(event => {
      if (event.isComposing || composing || event.keyCode === 229) return;
      if (event.key === 'Tab') {
        event.preventDefault(); commit();
        const result = runtime.key(0, Number(event.shiftKey));
        if (result !== 0) throw new Error(`Navigation failed: ${result}`);
        syncing = true; canvas.focus({ preventScroll: true }); syncing = false;
        sync(true);
      }
    }), options);
    editor.addEventListener('blur', guarded(event => {
      if (syncing || disposed) return;
      // Text edits arrive through input. Safari may reset the proxy value without
      // an input event when a restored page's editor loses focus.
      if (composing || editor.value === state?.text) {
        composing = false;
        commit();
      }
      compositionBase = null; drag = null;
      if (event.relatedTarget !== canvas && !owns(event.relatedTarget)) {
        const result = runtime.focus(0);
        if (result !== 0) throw new Error(`Focus failed: ${result}`);
        sync();
      }
    }), options);
    document.body.append(editor);
  }
  function sync(force = false) {
    if (disposed || !runtime.readText) return;
    const snapshot = runtime.readText();
    if (!snapshot || (!force && document.activeElement !== canvas && !owns(document.activeElement))) { remove(); return; }
    const focused = document.activeElement === canvas || owns(document.activeElement);
    // Replacing the DOM element cancels the old native composition. Its late
    // events cannot overwrite a newer model revision or a different page.
    const staleComposition = composing && (state?.session !== snapshot.session || state?.revision !== snapshot.revision || force);
    if (!editor || state?.session !== snapshot.session || state?.multiline !== snapshot.multiline || state?.password !== snapshot.password || staleComposition) create(snapshot);
    const bounds = canvas.getBoundingClientRect();
    const sx = bounds.width / canvas.width, sy = bounds.height / canvas.height;
    const inset = value => `${Math.max(0, value - 1)}px`;
    const bottom = snapshot.multiline ? snapshot.textBottom : snapshot.textTop + snapshot.lineHeight;
    Object.assign(editor.style, {
      zIndex: String((Number(canvas.style.zIndex) || 0) + 1),
      left: `${bounds.left + snapshot.left * sx}px`, top: `${bounds.top + snapshot.top * sy}px`,
      width: `${(snapshot.right - snapshot.left) * sx}px`, height: `${(snapshot.bottom - snapshot.top) * sy}px`,
      fontSize: `${snapshot.fontSize * sy}px`,
      paddingLeft: inset((snapshot.textLeft - snapshot.left) * sx),
      paddingRight: inset((snapshot.right - snapshot.textRight) * sx),
      paddingTop: inset((snapshot.textTop - snapshot.top) * sy),
      paddingBottom: inset((snapshot.bottom - bottom) * sy),
      lineHeight: `${snapshot.lineHeight * sy}px`,
      clipPath: `inset(${Math.max(0, snapshot.clipTop - snapshot.top) * sy}px ${Math.max(0, snapshot.right - snapshot.clipRight) * sx}px ${Math.max(0, snapshot.bottom - snapshot.clipBottom) * sy}px ${Math.max(0, snapshot.clipLeft - snapshot.left) * sx}px)`,
    });
    const authoritativeChange = force || !state || state.session !== snapshot.session || state.revision !== snapshot.revision;
    syncing = true;
    try {
      if (!composing && authoritativeChange) {
        if (editor.value !== snapshot.text) editor.value = snapshot.text;
        if (editor.selectionStart !== snapshot.selectionStart || editor.selectionEnd !== snapshot.selectionEnd ||
            (editor.selectionStart !== editor.selectionEnd && (editor.selectionDirection === 'backward') !== Boolean(snapshot.selectionBackward))) {
          editor.setSelectionRange(snapshot.selectionStart, snapshot.selectionEnd, snapshot.selectionBackward ? 'backward' : 'forward');
        }
      }
      state = snapshot;
      if (focused && document.activeElement !== editor) editor.focus({ preventScroll: true });
      editor.scrollLeft = snapshot.scrollX * sx; editor.scrollTop = snapshot.scrollY * sy;
    } finally { syncing = false; }
  }
  document.addEventListener('selectionchange', guarded(() => {
    if (owns(document.activeElement)) commit();
  }), { signal: events.signal });
  return {
    owns, sync, insertDrop,
    setLabel(name) { if (editor && name) editor.setAttribute('aria-label', name); },
    close() { disposed = true; events.abort(); remove(); },
  };
}
