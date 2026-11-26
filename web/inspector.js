// The browser handles transport and coordinates. Core owns the hit test.
export function startInspector(url, runtime, surfaces) {
  const endpoint = new URL('/__sunfleck/snapshot', url);
  const session = url.searchParams.get('sunfleckInspectorSession');
  if (session) endpoint.searchParams.set('session', session);
  const headers = { 'X-Sunfleck-Page': crypto.randomUUID() };
  const events = new AbortController();
  const options = { capture: true, signal: events.signal };
  let request, result, hover, timer;
  let reading = false;
  let lastSnapshot = -Infinity;

  function stopPicking() {
    if (!request) return;
    runtime.pick(false);
    request = undefined;
    hover = undefined;
    for (const canvas of surfaces.keys()) canvas.style.removeProperty('--sunfleck-pick-cursor');
  }

  function complete(selection) {
    result = { request, selection };
    stopPicking();
    lastSnapshot = -Infinity;
    wake();
  }

  function hit(event, action) {
    const canvas = document.elementsFromPoint(event.clientX, event.clientY).find(element => surfaces.has(element));
    if (!canvas) {
      if (hover !== undefined) runtime.pointer(hover, 1, -1, -1);
      hover = undefined;
      return false;
    }
    hover = surfaces.get(canvas);
    const box = canvas.getBoundingClientRect();
    const x = Math.floor((event.clientX - box.left) * canvas.width / box.width);
    const y = Math.floor((event.clientY - box.top) * canvas.height / box.height);
    return runtime.pointer(hover, action, x, y) === 0;
  }

  function suppress(event) {
    event.preventDefault();
    event.stopImmediatePropagation();
  }

  window.addEventListener('pointermove', event => {
    if (request) {
      hit(event, 1);
      suppress(event);
    }
  }, options);
  window.addEventListener('pointerout', event => {
    if (request && hover !== undefined && !event.relatedTarget) {
      runtime.pointer(hover, 1, -1, -1);
      hover = undefined;
    }
  }, options);
  for (const type of ['pointerdown', 'pointerup', 'click', 'contextmenu', 'wheel']) {
    window.addEventListener(type, event => {
      if (!request) return;
      suppress(event);
      if (type === 'click' && event.button === 0) {
        const selected = hit(event, 2) ? JSON.parse(runtime.snapshot() || '{}').selection : '0';
        complete(selected || '0');
      }
    }, { ...options, passive: false });
  }
  window.addEventListener('keydown', event => {
    if (!request) return;
    suppress(event);
    if (event.key === 'Escape') complete('0');
  }, options);

  async function exchange() {
    reading = true;
    try {
      if (performance.now() - lastSnapshot >= 900) {
        const raw = runtime.snapshot();
        if (raw) {
          const sentResult = result;
          const data = sentResult ? raw.slice(0, -1) + ',"browserPick":' + JSON.stringify(sentResult) + '}' : raw;
          const stream = new Blob([data]).stream().pipeThrough(new CompressionStream('gzip'));
          const response = await fetch(endpoint, {
            method: 'POST',
            headers: { ...headers, 'Content-Type': 'application/json', 'Content-Encoding': 'gzip' },
            body: await new Response(stream).blob(),
            signal: AbortSignal.any([events.signal, AbortSignal.timeout(1500)])
          });
          if (!response.ok) throw new Error('Sunfleck inspector snapshot upload failed');
          if (sentResult === result) lastSnapshot = performance.now();
        }
      }
      const response = await fetch(endpoint, {
        headers, cache: 'no-store', signal: AbortSignal.any([events.signal, AbortSignal.timeout(1000)])
      });
      if (!response.ok) throw new Error('Sunfleck inspector preview unavailable');
      const control = await response.json();
      if (events.signal.aborted) return;
      const next = control.pick !== result?.request ? control.pick : undefined;
      if (request && request !== next) stopPicking();
      if (next && !request && runtime.pick(true) === 0) {
        request = next;
        runtime.highlight('0');
      }
      for (const canvas of surfaces.keys()) {
        if (request) canvas.style.setProperty('--sunfleck-pick-cursor', 'crosshair');
      }
      if (!request) runtime.highlight(control.highlight || '0');
    } catch {
      if (!events.signal.aborted) {
        stopPicking();
        runtime.highlight('0');
      }
    } finally {
      reading = false;
      if (!events.signal.aborted) timer = setTimeout(exchange, 125);
    }
  }

  function wake() {
    clearTimeout(timer);
    if (!reading) void exchange();
  }
  document.addEventListener('visibilitychange', () => {
    if (!document.hidden) {
      lastSnapshot = -Infinity;
      wake();
    }
  }, options);
  wake();
  return () => {
    events.abort();
    clearTimeout(timer);
    stopPicking();
    runtime.highlight('0');
  };
}

