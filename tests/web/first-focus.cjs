const assert = require('node:assert/strict');
const path = require('node:path');
const fs = require('node:fs/promises');
const { createRequire } = require('node:module');
const dependencies = process.env.SUNFLECK_BROWSER_TEST_MODULES
  ? createRequire(path.resolve(process.env.SUNFLECK_BROWSER_TEST_MODULES, 'package.json')) : require;
const { chromium } = dependencies('playwright-core');

(async () => {
  const url = process.argv[2] || 'http://127.0.0.1:8767/';
  const evidence = process.argv[3] || 'out/web-evidence/first-focus';
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  const rounds = [];
  try {
    const page = await browser.newPage({ viewport: { width: 1000, height: 1000 } });
    const errors = [];
    page.on('pageerror', e => errors.push(String(e)));
    await page.addInitScript(() => {
      window.focusTrace = [];
      const name = element => element?.hasAttribute?.('data-sunfleck-editor') ? 'editor' : element?.id || element?.tagName || 'window';
      for (const type of ['focus', 'blur', 'pointerdown', 'pointerup', 'click', 'keydown', 'beforeinput', 'input', 'compositionstart', 'compositionend']) {
        window.addEventListener(type, event => {
          window.focusTrace.push({ type, target: name(event.target), active: name(document.activeElement),
            hasFocus: document.hasFocus(), key: event.key, composing: event.isComposing });
          if (window.focusTrace.length > 100) window.focusTrace.shift();
        }, true);
      }
    });
    const cdp = await page.context().newCDPSession(page);
    for (let round = 0; round < 6; ++round) {
      await page.goto(url);
      await page.waitForFunction(() => document.querySelector('#status')?.textContent === 'Ready', null, { timeout: 60000 });
      const bounds = await page.locator('#surface').boundingBox();
      // Do not use locator.fill/press/focus: they can hide a broken first-click focus.
      await page.mouse.click(bounds.x + 100, bounds.y + 266);
      const before = await page.evaluate(() => {
        const editor = document.querySelector('[data-sunfleck-editor]');
        return { value: editor?.value, first: editor?.selectionStart, last: editor?.selectionEnd };
      });
      assert.equal(typeof before.value, 'string', 'first click must create an editor');
      // No Shift, Ctrl+A, or refocusing action before the very first letter.
      await page.keyboard.type(`first${round}`);
      const current = () => page.evaluate(() => {
        const editor = document.querySelector('[data-sunfleck-editor]');
        return { focused: document.activeElement === editor, value: editor?.value };
      });
      const expected = before.value.slice(0, before.first) + `first${round}` + before.value.slice(before.last);
      assert.deepEqual(await current(), { focused: true, value: expected });
      await cdp.send('Input.imeSetComposition', { text: '候选', selectionStart: 2, selectionEnd: 2 });
      await cdp.send('Input.insertText', { text: '中文' });
      assert.deepEqual(await current(), { focused: true,
        value: before.value.slice(0, before.first) + `first${round}中文` + before.value.slice(before.last) });
      await page.locator('#status').click();
      await page.mouse.click(bounds.x + 100, bounds.y + 266);
      await page.keyboard.press('Control+A'); await page.keyboard.type('again');
      assert.deepEqual(await current(), { focused: true, value: 'again' });
      rounds.push({ round, events: await page.evaluate(() => window.focusTrace) });
    }
    assert.deepEqual(errors, []);
    await fs.mkdir(evidence, { recursive: true });
    const result = { result: 'passed', browser: browser.version(), url, rounds: rounds.length,
      ime: 'CDP only; does not reproduce native Windows IME activation state' };
    await fs.writeFile(path.join(evidence, 'result.json'), JSON.stringify(result, null, 2));
    await fs.writeFile(path.join(evidence, 'focus-trace.json'), JSON.stringify(rounds, null, 2));
    console.log(JSON.stringify(result));
  } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exitCode = 1; });
