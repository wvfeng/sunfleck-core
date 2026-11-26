const assert = require('node:assert/strict');
const path = require('node:path');
const fs = require('node:fs/promises');
const { createRequire } = require('node:module');
const dependencies = process.env.SUNFLECK_BROWSER_TEST_MODULES
  ? createRequire(path.resolve(process.env.SUNFLECK_BROWSER_TEST_MODULES, 'package.json')) : require;
const { chromium } = dependencies('playwright-core');

(async () => {
  const url = new URL('?scenario=workspace', process.argv[2] || 'http://127.0.0.1:8767/');
  const evidence = process.argv[3] || 'out/web-evidence/window-state';
  const browser = await chromium.launch({ channel: process.env.BROWSER_CHANNEL || 'msedge', headless: true });
  const errors = [];
  try {
    const page = await browser.newPage({ viewport: { width: 1000, height: 1000 }, deviceScaleFactor: 1 });
    page.on('pageerror', e => errors.push(String(e)));
    await page.goto(url.href);
    await page.waitForFunction(() => document.querySelector('#status').textContent === 'Ready', null, { timeout: 60000 });
    const activate = async name => {
      await page.locator(`[data-node-id][aria-label="${name}"]`).evaluate(e => e.click());
      await page.waitForTimeout(100);
    };
    await activate('Float document');
    const floating = page.locator('canvas[data-view-id]');
    await floating.waitFor();
    const original = await floating.boundingBox();
    const caption = async offset => {
      const b = await floating.boundingBox();
      await page.mouse.click(b.x + b.width - offset, b.y + 16);
      await page.waitForTimeout(150);
    };
    const sameBounds = async expected => {
      const actual = await floating.boundingBox();
      for (const key of ['x', 'y', 'width', 'height']) assert.ok(Math.abs(actual[key] - expected[key]) < 3,
        `${key}: expected ${expected[key]}, got ${actual[key]}`);
    };
    // Minimizing must not replace the normal rectangle used by maximize/restore.
    await caption(110); assert.ok((await floating.boundingBox()).height < 60);
    await caption(66); assert.ok((await floating.boundingBox()).height > original.height);
    await caption(66); await sameBounds(original);
    // Changing DPI while maximized must preserve the logical restore rectangle.
    await caption(66);
    const cdp = await page.context().newCDPSession(page);
    await cdp.send('Emulation.setDeviceMetricsOverride', { width: 1000, height: 1000, deviceScaleFactor: 2, mobile: false });
    await page.waitForFunction(() => window.devicePixelRatio === 2 && document.querySelector('canvas[data-view-id]').width > 1000);
    await caption(66); await sameBounds(original);
    // A maximized panel follows its parent when the viewport width changes.
    await caption(66);
    await cdp.send('Emulation.setDeviceMetricsOverride', { width: 760, height: 900, deviceScaleFactor: 2, mobile: false });
    await page.waitForTimeout(250);
    const main = await page.locator('#surface').boundingBox();
    assert.ok(Math.abs((await floating.boundingBox()).width - main.width) < 3);
    await caption(66);
    // Escape restores the origin of a title drag and releases capture.
    const beforeDrag = await floating.boundingBox();
    await page.mouse.move(beforeDrag.x + 80, beforeDrag.y + 16); await page.mouse.down();
    await page.mouse.move(beforeDrag.x + 140, beforeDrag.y + 56, { steps: 6 });
    await page.keyboard.press('Escape'); await page.mouse.up();
    await page.waitForTimeout(150); await sameBounds(beforeDrag);
    // Corner resize cancellation follows the same transaction as title dragging.
    await page.mouse.move(beforeDrag.x + beforeDrag.width - 2, beforeDrag.y + beforeDrag.height - 2);
    await page.mouse.down();
    await page.mouse.move(beforeDrag.x + beforeDrag.width + 38, beforeDrag.y + beforeDrag.height + 38, { steps: 6 });
    await page.keyboard.press('Escape'); await page.mouse.up();
    await page.waitForTimeout(150); await sameBounds(beforeDrag);
    // A live DPI transition cancels an in-flight resize before applying the new scale.
    await page.mouse.move(beforeDrag.x + beforeDrag.width - 2, beforeDrag.y + beforeDrag.height - 2);
    await page.mouse.down();
    await page.mouse.move(beforeDrag.x + beforeDrag.width + 38, beforeDrag.y + beforeDrag.height + 38, { steps: 6 });
    await cdp.send('Emulation.setDeviceMetricsOverride', { width: 760, height: 900, deviceScaleFactor: 1.25, mobile: false });
    await page.waitForTimeout(250); await page.mouse.up(); await sameBounds(beforeDrag);
    // A minimized window retains its full restore height across another DPI change.
    await caption(110);
    await cdp.send('Emulation.setDeviceMetricsOverride', { width: 760, height: 900, deviceScaleFactor: 2, mobile: false });
    await page.waitForTimeout(250);
    await caption(110); await sameBounds(beforeDrag);
    // Short CJK drafts exercise view-scoped composition and short-string storage.
    await activate('documentText');
    const editor = page.locator('[data-sunfleck-editor]');
    await editor.fill('');
    await cdp.send('Input.imeSetComposition', { text: '候选', selectionStart: 2, selectionEnd: 2 });
    assert.equal(await editor.inputValue(), '候选');
    await cdp.send('Input.insertText', { text: '中文' });
    assert.equal(await editor.inputValue(), '中文');
    await activate('Reset workspace'); await activate('documentText');
    assert.equal(await editor.inputValue(), '中文');
    await fs.mkdir(evidence, { recursive: true });
    await page.screenshot({ path: path.join(evidence, 'restored.png') });
    await page.locator('#stop').click();
    assert.deepEqual(errors, []);
    const result = { result: 'passed', browser: browser.version(), url: url.href,
      checks: ['minimize/maximize/restore', 'live DPR 1/1.25/2', 'maximized viewport resize', 'Escape move/resize cancellation', 'DPI change during resize', 'minimized DPI restore', 'floating CJK composition and docking'],
      ime: 'CDP composition; no new OS candidate-window acceptance' };
    await fs.writeFile(path.join(evidence, 'result.json'), JSON.stringify(result, null, 2));
    console.log(JSON.stringify(result));
  } catch (e) { console.error(errors); throw e; }
  finally { await browser.close(); }
})().catch(e => { console.error(e); process.exitCode = 1; });
