const assert = require('node:assert/strict');
const path = require('node:path');
const fs = require('node:fs/promises');
const { createRequire } = require('node:module');
const dependencies = process.env.SUNFLECK_BROWSER_TEST_MODULES
  ? createRequire(path.resolve(process.env.SUNFLECK_BROWSER_TEST_MODULES, 'package.json')) : require;
const { chromium } = dependencies('playwright-core');
(async () => {
  const url = new URL('?scenario=workspace', process.argv[2]);
  const evidence = process.argv[3];
  const browser = await chromium.launch({ channel: process.env.BROWSER_CHANNEL || 'msedge', headless: true });
  try {
    const page = await browser.newPage({ viewport: { width: 1000, height: 1000 } });
    const errors = []; page.on('pageerror', error => errors.push(String(error)));
    const cdp = await page.context().newCDPSession(page);
    await cdp.send('Network.enable'); await cdp.send('Network.setCacheDisabled', { cacheDisabled: true });
    await page.goto(url.href);
    await page.waitForFunction(() => document.querySelector('#status').textContent === 'Ready', null, { timeout: 60000 });
    const cold = await page.evaluate(() => ({ readyMs: performance.now(),
      transferredBytes: performance.getEntriesByType('resource').reduce((n, r) => n + r.transferSize, 0),
      decodedBytes: performance.getEntriesByType('resource').reduce((n, r) => n + r.decodedBodySize, 0) }));
    await page.waitForSelector('[aria-label="Enable option"]');
    const latency = await page.evaluate(async () => {
      const values = [];
      const button = document.querySelector('[aria-label="Enable option"]');
      for (let i = 0; i < 30; ++i) {
        values.push(await new Promise(resolve => {
          const previous = button.getAttribute('aria-checked'), start = performance.now();
          const observer = new MutationObserver(() => {
            if (button.getAttribute('aria-checked') !== previous) { observer.disconnect(); resolve(performance.now() - start); }
          });
          observer.observe(button, { attributes: true, attributeFilter: ['aria-checked'] });
          button.click();
        }));
      }
      return values.sort((a,b) => a-b);
    });
    await page.locator('[aria-label="documentText"]').evaluate(e => e.click());
    const editor = page.locator('[data-sunfleck-editor]');
    const large = Array.from({ length: 500 }, (_, i) => `第 ${i} 行：中文与 emoji 🌱 — long text editing`).join('\n');
    await editor.fill(large); await editor.press('Control+End');
    await page.locator('[aria-label="Float document"]').evaluate(e => e.click());
    await page.waitForSelector('canvas[data-view-id]');
    await page.locator('[aria-label="documentText"]').evaluate(e => e.click());
    assert.equal(await editor.inputValue(), large);
    await editor.press('Control+End');
    await page.keyboard.insertText('末尾');
    assert.equal(await editor.inputValue(), large + '末尾');
    const memory = await page.evaluate(() => performance.memory?.usedJSHeapSize ?? null);
    await page.locator('#stop').click();
    assert.deepEqual(errors, []);
    const result = { result: 'passed', browser: browser.version(), url: url.href, cold,
      inputToPresentedSemanticsMs: { median: latency[15], p95: latency[28] }, jsHeapBytes: memory, stressLines: 500,
      note: 'Local HTTP, headless Edge, one run. Action-to-semantic publication follows Canvas upload; JS heap excludes some native/GPU allocations.' };
    await fs.mkdir(evidence, { recursive: true });
    await fs.writeFile(path.join(evidence, 'result.json'), JSON.stringify(result, null, 2));
    console.log(JSON.stringify(result));
  } finally { await browser.close(); }
})().catch(e => { console.error(e); process.exitCode = 1; });
