const assert = require('node:assert/strict');
const path = require('node:path');
const fs = require('node:fs/promises');
const { createRequire } = require('node:module');
const dependencies = process.env.SUNFLECK_BROWSER_TEST_MODULES
  ? createRequire(path.resolve(process.env.SUNFLECK_BROWSER_TEST_MODULES, 'package.json')) : require;
const { chromium } = dependencies('playwright-core');

(async () => {
  const url = new URL(process.argv[2]); const evidence = process.argv[3];
  const browser = await chromium.launch({ channel: process.env.BROWSER_CHANNEL || 'msedge', headless: true });
  const errors = [];
  try {
    const page = await browser.newPage({ viewport: { width: 1000, height: 1000 } });
    page.on('pageerror', e => errors.push(String(e)));
    page.on('console', e => { if (e.type() === 'error') errors.push(e.text()); });
    const open = async mode => {
      url.searchParams.set('renderer', mode); await page.goto(url.href);
      await page.waitForFunction(() => document.querySelector('#status').textContent !== 'Loading…', null, { timeout: 60000 });
      assert.equal(await page.locator('#status').textContent(), 'Ready');
      await page.waitForSelector(`canvas[data-renderer="${mode}"]`);
      await page.waitForSelector('[data-node-id]');
    };
    const snapshot = () => page.locator('#surface').evaluate(c => {
      const copy = document.createElement('canvas'); copy.width = c.width; copy.height = c.height;
      const ctx = copy.getContext('2d'); ctx.drawImage(c, 0, 0);
      return Array.from(ctx.getImageData(0, 0, c.width, c.height).data);
    });
    await open('canvas2d'); const cpu = await snapshot();
    await open('webgl2'); const gpu = await snapshot();
    assert.equal(gpu.length, cpu.length);
    let maxDifference = 0, changed = 0;
    for (let i = 0; i < cpu.length; ++i) {
      const diff = Math.abs(cpu[i] - gpu[i]); maxDifference = Math.max(maxDifference, diff); if (diff) ++changed;
    }
    // Fixed-function UNORM blend rounding can differ by a few levels at AA edges.
    assert.ok(maxDifference <= 3, `CPU/WebGL2 max channel difference ${maxDifference}`);
    await fs.mkdir(evidence, { recursive: true });
    await page.screenshot({ path: path.join(evidence, 'webgl2.png') });
    const stats = await page.locator('#surface').evaluate(c => ({ ...c.dataset }));
    assert.ok(Number(stats.rectangles) > 0); assert.equal(Number(stats.drawCalls), Math.ceil(Number(stats.rectangles) / 65536));
    // Real host restores an idle frame without requesting a fresh core frame.
    await page.evaluate(() => {
      window.loss = document.querySelector('#surface').getContext('webgl2').getExtension('WEBGL_lose_context');
      if (!window.loss) throw new Error('Context-loss testing extension unavailable');
      window.loss.loseContext();
    });
    await page.waitForSelector('#surface[data-renderer-state="lost"]');
    await page.evaluate(() => window.loss.restoreContext());
    await page.waitForSelector('#surface[data-renderer-state="ready"]').catch(error => { throw new Error(`${error}\n${errors.join('\n')}`); });
    assert.deepEqual(await snapshot(), gpu);
    await page.locator('#stop').click();
    assert.equal(await page.locator('#surface').getAttribute('data-renderer-state'), 'closed');

    const synthetic = await page.evaluate(async () => {
      const { createRenderer } = await import('./renderer.js');
      const canvas = document.createElement('canvas'); canvas.width = 16; canvas.height = 16;
      const failures = [];
      const renderer = createRenderer(canvas, 'webgl2', e => failures.push(String(e)));
      const gl = canvas.getContext('webgl2');
      const read = (x, y) => { const p = new Uint8Array(4); gl.readPixels(x, 15-y, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, p); return Array.from(p); };
      const bytes = new Uint8Array(65538 * 20); const wire = new DataView(bytes.buffer);
      const rect = (i, box, rgba) => { box.forEach((n, j) => wire.setInt32(i * 20 + j * 4, n, true)); bytes.set(rgba, i * 20 + 16); };
      rect(0, [-2,-2,18,18], [20,40,60,255]);
      // Empty intermediate rectangles force the final blend across a batch boundary.
      rect(65536, [2,3,7,9], [220,140,60,128]); rect(65537, [4,5,6,7], [0,0,200,128]);
      renderer.present(bytes);
      const result = { background: read(0,0), one: read(2,3), two: read(4,5), calls: canvas.dataset.drawCalls };
      const loss = gl.getExtension('WEBGL_lose_context');
      const wait = event => new Promise((resolve, reject) => {
        const timer = setTimeout(() => reject(new Error(`Timed out: ${event}; ${failures.join('; ')}`)), 10000);
        canvas.addEventListener(event, () => { clearTimeout(timer); resolve(); }, { once: true });
      });
      let done = wait('webglcontextlost'); loss.loseContext(); await done;
      rect(0, [-2,-2,18,18], [70,80,90,255]); renderer.present(bytes); bytes.fill(0);
      // Restore after the lost event has fully returned, not in its microtask.
      await new Promise(resolve => setTimeout(resolve, 50));
      done = wait('webglcontextrestored'); loss.restoreContext(); await done;
      result.restored = read(0,0); result.failures = failures;
      const program = gl.getParameter(gl.CURRENT_PROGRAM); renderer.close(); renderer.close();
      // Simulate unavailable WebGL2 before context creation, exercising auto fallback.
      const fallback = document.createElement('canvas'); fallback.width = fallback.height = 1;
      const original = fallback.getContext.bind(fallback);
      fallback.getContext = (kind, options) => kind === 'webgl2' ? null : original(kind, options);
      const cpu = createRenderer(fallback); cpu.present(new Uint8Array([9,8,7,255]));
      result.fallback = { kind: cpu.kind, pixel: Array.from(original('2d').getImageData(0,0,1,1).data) }; cpu.close();
      try { createRenderer(fallback, 'webgl2'); result.strictRejected = false; } catch { result.strictRejected = true; }
      gl.useProgram(null); result.released = !gl.isProgram(program);
      return result;
    });
    assert.deepEqual(synthetic.background, [20,40,60,255]);
    assert.deepEqual(synthetic.one, [120,90,60,255]);
    assert.ok(synthetic.two.every((v,i) => Math.abs(v-[60,45,130,255][i]) <= 1));
    assert.equal(synthetic.calls, '2'); assert.deepEqual(synthetic.restored, [70,80,90,255]);
    assert.deepEqual(synthetic.failures, []); assert.equal(synthetic.released, true);
    assert.deepEqual(synthetic.fallback, { kind: 'canvas2d', pixel: [9,8,7,255] }); assert.equal(synthetic.strictRejected, true);

    // Auto fallback must work end-to-end with the actual Wasm host as well.
    await page.addInitScript(() => {
      const original = HTMLCanvasElement.prototype.getContext;
      HTMLCanvasElement.prototype.getContext = function(kind, ...args) {
        return kind === 'webgl2' ? null : original.call(this, kind, ...args);
      };
    });
    url.searchParams.set('renderer', 'auto'); await page.goto(url.href);
    await page.waitForSelector('#surface[data-renderer="canvas2d"]');
    await page.waitForSelector('[data-node-id]'); assert.deepEqual(await snapshot(), cpu);
    await page.locator('#stop').click(); assert.deepEqual(errors, []);
    const result = { result: 'passed', browser: browser.version(), maxDifference, changedChannels: changed, stats, synthetic };
    await fs.writeFile(path.join(evidence, 'result.json'), JSON.stringify(result, null, 2)); console.log(JSON.stringify(result));
  } finally { await browser.close(); }
})().catch(e => { console.error(e); process.exitCode = 1; });
