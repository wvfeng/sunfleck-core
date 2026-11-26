// Ordered FillRectCommand v1 stream shared by C++ and .NET Wasm.
// Geometry, glyph coverage and clipping come from the core; WebGL2 composites.
const STRIDE = 20;
const BATCH_RECTS = 65536;
const vertexSource = `#version 300 es
precision highp float;
precision highp int;
layout(location=0) in ivec4 bounds;
layout(location=1) in vec4 color;
uniform vec2 surfaceSize;
out vec4 tint;
const vec2 corners[6] = vec2[6](vec2(0,0),vec2(1,0),vec2(0,1),vec2(0,1),vec2(1,0),vec2(1,1));
void main() {
  vec2 point = mix(vec2(bounds.xy), vec2(bounds.zw), corners[gl_VertexID]);
  gl_Position = vec4(point / surfaceSize * vec2(2,-2) + vec2(-1,1), 0, 1);
  tint = color;
}`;
const fragmentSource = `#version 300 es
precision highp float;
in vec4 tint;
out vec4 pixel;
void main() { pixel = tint; }`;

export function createRenderer(canvas, requested = 'auto', onError = console.error) {
  if (!['auto', 'webgl2', 'canvas2d'].includes(requested)) throw new Error(`Unknown renderer: ${requested}`);
  const gl = requested === 'canvas2d' ? null : canvas.getContext('webgl2', {
    alpha: true, premultipliedAlpha: true, antialias: false, depth: false, stencil: false,
    // Dirty-frame presentation must survive idle frames and canvas snapshots.
    preserveDrawingBuffer: true,
  });
  if (!gl) {
    if (requested === 'webgl2') throw new Error('WebGL2 is unavailable');
    const context = canvas.getContext('2d');
    if (!context) throw new Error('Canvas 2D is unavailable');
    canvas.dataset.renderer = 'canvas2d'; canvas.dataset.rendererState = 'ready';
    return { kind: 'canvas2d', present(bytes) {
      if (bytes.length !== canvas.width * canvas.height * 4) throw new Error('Invalid pixel buffer');
      context.putImageData(new ImageData(new Uint8ClampedArray(bytes), canvas.width, canvas.height), 0, 0);
    }, close() { canvas.dataset.rendererState = 'closed'; } };
  }
  let program = null, buffer = null, vao = null, sizeUniform = null;
  let capacity = 0, last = null, lost = false, closed = false;
  const events = new AbortController();
  const shaders = [];
  function release() {
    for (const shader of shaders) gl.deleteShader(shader);
    shaders.length = 0;
    if (buffer) gl.deleteBuffer(buffer);
    if (vao) gl.deleteVertexArray(vao);
    if (program) gl.deleteProgram(program);
    program = buffer = vao = sizeUniform = null; capacity = 0;
  }
  function initialize() {
    try {
      program = gl.createProgram();
      if (!program) throw new Error('Cannot allocate WebGL2 program');
      for (const [type, source] of [[gl.VERTEX_SHADER, vertexSource], [gl.FRAGMENT_SHADER, fragmentSource]]) {
        const shader = gl.createShader(type);
        if (!shader) throw new Error('Cannot allocate WebGL2 shader');
        shaders.push(shader); gl.shaderSource(shader, source); gl.compileShader(shader);
        if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) throw new Error(gl.getShaderInfoLog(shader));
        gl.attachShader(program, shader);
      }
      gl.linkProgram(program);
      if (!gl.getProgramParameter(program, gl.LINK_STATUS)) throw new Error(gl.getProgramInfoLog(program));
      for (const shader of shaders) { gl.detachShader(program, shader); gl.deleteShader(shader); }
      shaders.length = 0;
      vao = gl.createVertexArray(); buffer = gl.createBuffer();
      if (!vao || !buffer) throw new Error('Cannot allocate WebGL2 rectangle buffer');
      sizeUniform = gl.getUniformLocation(program, 'surfaceSize');
      gl.bindVertexArray(vao); gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
      gl.enableVertexAttribArray(0); gl.vertexAttribIPointer(0, 4, gl.INT, STRIDE, 0); gl.vertexAttribDivisor(0, 1);
      gl.enableVertexAttribArray(1); gl.vertexAttribPointer(1, 4, gl.UNSIGNED_BYTE, true, STRIDE, 16); gl.vertexAttribDivisor(1, 1);
      gl.disable(gl.DEPTH_TEST); gl.disable(gl.CULL_FACE); gl.disable(gl.SCISSOR_TEST); gl.disable(gl.DITHER);
      gl.enable(gl.BLEND); gl.blendEquation(gl.FUNC_ADD);
      // Source colors are straight; the framebuffer stores premultiplied RGBA.
      gl.blendFuncSeparate(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA, gl.ONE, gl.ONE_MINUS_SRC_ALPHA);
    } catch (error) { release(); throw error; }
  }
  function draw() {
    if (closed || lost || gl.isContextLost() || !last) return;
    gl.viewport(0, 0, canvas.width, canvas.height);
    gl.clearColor(0, 0, 0, 0); gl.clear(gl.COLOR_BUFFER_BIT);
    gl.useProgram(program); gl.bindVertexArray(vao); gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
    gl.uniform2f(sizeUniform, canvas.width, canvas.height);
    const required = Math.min(last.length, BATCH_RECTS * STRIDE);
    if (required > capacity) {
      capacity = Math.max(required, Math.min(BATCH_RECTS * STRIDE, Math.max(4096, capacity * 2)));
      gl.bufferData(gl.ARRAY_BUFFER, capacity, gl.DYNAMIC_DRAW);
    }
    let calls = 0;
    for (let offset = 0; offset < last.length; offset += BATCH_RECTS * STRIDE) {
      const batch = last.subarray(offset, offset + BATCH_RECTS * STRIDE);
      gl.bufferSubData(gl.ARRAY_BUFFER, 0, batch);
      gl.drawArraysInstanced(gl.TRIANGLES, 0, 6, batch.length / STRIDE); ++calls;
    }
    const error = gl.getError();
    if (error !== gl.NO_ERROR && error !== gl.CONTEXT_LOST_WEBGL) throw new Error(`WebGL2 draw failed: ${error}`);
    canvas.dataset.drawCalls = String(calls); canvas.dataset.rectangles = String(last.length / STRIDE);
  }
  canvas.addEventListener('webglcontextlost', event => {
    event.preventDefault(); lost = true; canvas.dataset.rendererState = 'lost';
  }, { signal: events.signal });
  canvas.addEventListener('webglcontextrestored', () => {
    if (closed) return;
    try {
      // Handles from the lost generation are already invalid; don't delete them
      // against the newly restored context (that generates INVALID_OPERATION).
      program = buffer = vao = sizeUniform = null; capacity = 0; shaders.length = 0;
      initialize(); lost = false; draw(); canvas.dataset.rendererState = 'ready';
    } catch (error) { onError(error); }
  }, { signal: events.signal });
  try { initialize(); } catch (error) { events.abort(); throw error; }
  canvas.dataset.renderer = 'webgl2'; canvas.dataset.rendererState = 'ready';
  return { kind: 'webgl2', present(bytes) {
    if (closed) return;
    if (!(bytes instanceof Uint8Array) || bytes.length % STRIDE) throw new Error('Invalid draw command buffer');
    // Own the latest frame for restore; never retain a view into growing Wasm memory.
    last = bytes.slice(); draw();
  }, close() {
    if (closed) return;
    closed = true; events.abort(); release(); last = null; canvas.dataset.rendererState = 'closed';
  } };
}
