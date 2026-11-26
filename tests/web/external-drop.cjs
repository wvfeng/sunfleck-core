const assert = require('node:assert/strict');
const path = require('node:path');
const fs = require('node:fs/promises');
const { createRequire } = require('node:module');
const dependencies = process.env.SUNFLECK_BROWSER_TEST_MODULES
  ? createRequire(path.resolve(process.env.SUNFLECK_BROWSER_TEST_MODULES, 'package.json')) : require;
const { chromium } = dependencies('playwright-core');

(async () => {
  const url = new URL('?scenario=workspace', process.argv[2]);
  const evidence = path.resolve(process.argv[3] || 'out/web-evidence/external-drop');
  await fs.mkdir(evidence, { recursive: true });
  const testFile = path.join(evidence, 'Sunfleck drop.txt');
  const contents = 'external file drop\n';
  await fs.writeFile(testFile, contents);
  const browser = await chromium.launch({ channel: process.env.BROWSER_CHANNEL || 'msedge', headless: true });
  try {
    const page = await browser.newPage({ viewport: { width: 1000, height: 1000 } });
    await page.goto(url.href);
    await page.waitForFunction(() => {
      const status = document.querySelector('#status')?.textContent;
      return status && status !== 'Loading…';
    });
    assert.equal(await page.locator('#status').textContent(), 'Ready');
    const cdp = await page.context().newCDPSession(page);
    const drop = async (locator, data, fraction = .5) => {
      const { x, y, width, height } = await locator.boundingBox();
      for (const type of ['dragEnter', 'dragOver', 'drop']) {
        await cdp.send('Input.dispatchDragEvent', {
          type, x: Math.round(x + width * fraction), y: Math.round(y + height / 2),
          data: { ...data, dragOperationsMask: 1 },
        });
      }
    };
    const report = page.locator('#file-drop-status');
    const expected = `Sunfleck drop.txt (${Buffer.byteLength(contents)} B)`;
    const dropArea = page.getByRole('region', { name: 'Drop a file from File Explorer here' });
    assert.ok(await dropArea.isVisible());
    await drop(dropArea.locator('strong'), { items: [], files: [testFile] });
    await page.waitForFunction(value => document.querySelector('#file-drop-status')?.textContent === value, expected);
    await report.evaluate(element => { element.textContent = ''; });
    await drop(page.locator('#surface'), { items: [], files: [testFile] }, .8);
    await page.waitForFunction(value => document.querySelector('#file-drop-status')?.textContent === value, expected);
    await report.evaluate(element => { element.textContent = ''; });
    await page.locator('[data-node-id][aria-label="search"]').evaluate(element => element.click());
    const input = page.locator('[data-sunfleck-editor]');
    await input.waitFor();
    const beforeFile = await input.inputValue();
    await drop(input, { items: [], files: [testFile] });
    await page.waitForFunction(value => document.querySelector('#file-drop-status')?.textContent === value, expected);
    assert.equal(await input.inputValue(), beforeFile, 'file handling leaves editor text intact');
    await drop(input, { items: [{ mimeType: 'text/plain', data: 'external text' }] });
    assert.match(await input.inputValue(), /external text/);
    await page.locator('#status').click();
    await input.waitFor({ state: 'detached' });
    const search = page.locator('[data-node-id][aria-label="search"]');
    await drop(search, { items: [{ mimeType: 'text/plain', data: 'unfocused drop' }] });
    await input.waitFor();
    assert.match(await input.inputValue(), /unfocused drop/);
    assert.equal(page.url(), url.href);
    const result = { url: url.href, browser: browser.version(), result: 'passed',
      checks: ['external file on labeled drop area', 'external file on canvas', 'external file on active editor',
        'external text on active editor', 'external text on unfocused editor'] };
    await fs.writeFile(path.join(evidence, 'result.json'), JSON.stringify(result, null, 2));
    console.log(JSON.stringify(result));
  } finally {
    await browser.close();
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
