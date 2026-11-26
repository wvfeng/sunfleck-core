const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const path = require('node:path');
const { createRequire } = require('node:module');
const deps = process.env.SUNFLECK_BROWSER_TEST_MODULES ? createRequire(path.resolve(process.env.SUNFLECK_BROWSER_TEST_MODULES, 'package.json')) : require;
const { chromium } = deps('playwright-core');
(async () => {
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  try {
    const page = await browser.newPage({ viewport: { width: 1000, height: 1000 } });
    const url = process.argv[2] || 'http://127.0.0.1:8767/';
    await page.goto(url);
    await page.waitForFunction(() => document.querySelector('#status')?.textContent === 'Ready');
    assert.equal(await page.evaluate(() => window.sunfleckInputDiagnostics), undefined);
    await page.goto(url + '?inputDiagnostics=1');
    await page.waitForFunction(() => document.querySelector('#status')?.textContent === 'Ready');
    const rect = await page.locator('#surface').boundingBox();
    await page.mouse.click(rect.x + 100, rect.y + 266);
    await page.keyboard.type('private-sample');
    const report = await page.evaluate(() => window.sunfleckInputDiagnostics.report());
    assert.ok(report.events.some(e => e.type === 'input'));
    assert.ok(report.events.some(e => e.target === 'editor'));
    assert.equal(JSON.stringify(report).includes('private-sample'), false);
    await page.evaluate(() => {
      const editor = document.querySelector('[data-sunfleck-editor]');
      for (let i = 0; i < 250; ++i) editor.dispatchEvent(new KeyboardEvent('keyup', { key: 'a', bubbles: true }));
    });
    assert.equal(await page.evaluate(() => window.sunfleckInputDiagnostics.events.length), 200);
    await fs.mkdir('out/web-evidence/input-diagnostics', { recursive: true });
    await fs.writeFile('out/web-evidence/input-diagnostics/result.json', JSON.stringify({ result: 'passed', browser: browser.version(),
      checks: ['disabled-by-default', 'captures-input-and-focus', 'no-typed-text', 'bounded-200-events'] }, null, 2));
    console.log('Input diagnostics: 4 checks passed.');
  } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exitCode = 1; });
