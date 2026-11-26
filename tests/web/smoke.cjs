// Run against a served prototype using an installed Edge (or set BROWSER_CHANNEL).
const assert = require('node:assert/strict');
const path = require('node:path');
const fs = require('node:fs/promises');
const { createRequire } = require('node:module');
const dependencies = process.env.SUNFLECK_BROWSER_TEST_MODULES
  ? createRequire(path.resolve(process.env.SUNFLECK_BROWSER_TEST_MODULES, 'package.json')) : require;
const { chromium } = dependencies('playwright-core');

(async () => {
  const url = process.argv[2] || 'http://127.0.0.1:8765/';
  const evidence = process.argv[3] || 'out/web-evidence';
  const browser = await chromium.launch({ channel: process.env.BROWSER_CHANNEL || 'msedge', headless: true });
  const errors = [];
  try {
    const page = await browser.newPage({ viewport: { width: 1000, height: 700 }, deviceScaleFactor: 1 });
    page.on('pageerror', error => errors.push(String(error)));
    page.on('console', message => { if (message.type() === 'error') errors.push(message.text()); });
    await page.goto(url);
    await page.waitForFunction(() => document.querySelector('#status')?.textContent !== 'Loading…', null, { timeout: 60000 });
    assert.equal(await page.locator('#status').textContent(), 'Ready');
    await page.waitForFunction(() => document.querySelector('canvas').width > 300);
    await fs.mkdir(evidence, { recursive: true });
    await page.screenshot({ path: path.join(evidence, 'initial.png') });
    // Pixel comparison checks actual rendered state without depending on runtime internals.
    const pixels = () => page.locator('canvas').evaluate(canvas => canvas.toDataURL());
    const initial = await pixels();
    const canvas = page.locator('canvas');
    await canvas.click({ position: { x: 80, y: 170 } });
    await page.mouse.move(10, 10);
    await page.waitForTimeout(100);
    const once = await pixels();
    assert.ok(once !== initial, 'real pointer input should update the canvas');
    await page.screenshot({ path: path.join(evidence, 'increment.png') });
    // Start async work, then interact with the synchronous button while it runs.
    await canvas.click({ position: { x: 80, y: 218 } });
    await canvas.click({ position: { x: 80, y: 218 } });
    await canvas.click({ position: { x: 80, y: 170 } });
    await page.mouse.move(10, 10);
    await page.waitForTimeout(750);
    const asyncResult = await pixels();
    assert.ok(asyncResult !== once);
    await page.screenshot({ path: path.join(evidence, 'async.png') });
    // Match against three ordinary increments on a fresh instance: the async
    // double-click must execute once, and the synchronous click must still run.
    await page.reload();
    await page.waitForFunction(() => document.querySelector('#status')?.textContent === 'Ready');
    for (let i = 0; i < 3; ++i) await canvas.click({ position: { x: 80, y: 170 } });
    await page.mouse.move(10, 10);
    await page.waitForTimeout(100);
    assert.ok(await pixels() === asyncResult, 'async single-flight and responsive synchronous action');
    // The clicked synchronous button retains core focus; Enter must activate it.
    await canvas.press('Enter');
    await page.waitForTimeout(100);
    assert.ok(await pixels() !== asyncResult, 'keyboard activation should update the canvas');
    const oldWidth = await canvas.evaluate(element => element.width);
    await page.setViewportSize({ width: 700, height: 600 });
    await page.waitForFunction(width => document.querySelector('canvas').width < width, oldWidth);
    assert.equal(await page.locator('#status').textContent(), 'Ready');
    // Close with a callback outstanding; a late completion must not touch App.
    await canvas.click({ position: { x: 80, y: 218 } });
    await page.locator('#stop').click();
    await page.waitForTimeout(750);
    assert.equal(await page.locator('#status').textContent(), 'Closed');
    const scaled = await browser.newPage({ viewport: { width: 700, height: 600 }, deviceScaleFactor: 2 });
    scaled.on('pageerror', error => errors.push(String(error)));
    scaled.on('console', message => { if (message.type() === 'error') errors.push(message.text()); });
    await scaled.goto(url);
    await scaled.waitForFunction(() => document.querySelector('#status')?.textContent === 'Ready');
    await scaled.waitForFunction(() => {
      const element = document.querySelector('canvas');
      return element.width === Math.round(element.getBoundingClientRect().width * 2);
    });
    await scaled.locator('canvas').click({ position: { x: 80, y: 170 } });
    await scaled.waitForTimeout(100);
    await scaled.screenshot({ path: path.join(evidence, 'dpr2.png') });
    await scaled.locator('#stop').click();
    assert.equal(await scaled.locator('#status').textContent(), 'Closed');
    assert.deepEqual(errors, []);
    const result = { url, browser: browser.version(), result: 'passed' };
    await fs.writeFile(path.join(evidence, 'result.json'), JSON.stringify(result, null, 2));
    console.log(JSON.stringify(result));
  } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exitCode = 1; });
