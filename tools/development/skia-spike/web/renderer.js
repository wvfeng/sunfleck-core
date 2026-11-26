// Replaces only the copied evaluation site's renderer.js.
export function createRenderer(canvas, requested = 'auto', onError = console.error) {
  const bridge = globalThis.skiaEvaluationBridge;
  if (!bridge || requested === 'canvas2d') throw new Error('This evaluation requires the native Skia WebGL bridge');
  if (bridge.init() !== 0) throw new Error('Skia WebGL initialization failed');
  let closed = false, lost = false, last = null;
  const events = new AbortController();
  canvas.dataset.renderer = 'skia-webgl2'; canvas.dataset.rendererState = 'ready';
  function draw() {
    if (closed || lost || !last) return;
    const result = bridge.present(last, canvas.width, canvas.height);
    if (result !== 0) throw new Error(`Skia present failed: ${result}`);
    canvas.dataset.rectangles = String(last.length / 20);
  }
  canvas.addEventListener('webglcontextlost', event => {
    event.preventDefault(); lost = true; bridge.lost(); canvas.dataset.rendererState = 'lost';
  }, { signal: events.signal });
  canvas.addEventListener('webglcontextrestored', () => {
    try {
      if (closed) return;
      if (bridge.restore() !== 0) throw new Error('Skia context restore failed');
      lost = false; draw(); canvas.dataset.rendererState = 'ready';
    } catch (error) { onError(error); }
  }, { signal: events.signal });
  return { kind: 'webgl2', present(bytes) {
    if (closed) return;
    if (bytes.length % 20) throw new Error('Invalid rectangle stream');
    last = bytes.slice(); globalThis.skiaEvaluationLastCommands = last; draw();
  }, close() {
    if (closed) return;
    closed = true; events.abort(); bridge.close(); last = null; canvas.dataset.rendererState = 'closed';
  } };
}
