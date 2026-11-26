const assert = require('node:assert/strict');
const path = require('node:path');
const fs = require('node:fs/promises');
const { createRequire } = require('node:module');
const dependencies = process.env.SUNFLECK_BROWSER_TEST_MODULES
  ? createRequire(path.resolve(process.env.SUNFLECK_BROWSER_TEST_MODULES, 'package.json')) : require;
const { chromium } = dependencies('playwright-core');

(async () => {
  const url = process.argv[2] || 'http://127.0.0.1:8767/';
  const evidence = process.argv[3] || 'out/web-evidence/text';
  const browser = await chromium.launch({ channel: process.env.BROWSER_CHANNEL || 'msedge', headless: true });
  const errors = [];
  try {
    const dpr = Number(process.env.SUNFLECK_TEST_DPR || 1);
    const page = await browser.newPage({ viewport: { width: 1000, height: 1000 }, deviceScaleFactor: dpr });
    page.on('pageerror', error => errors.push(String(error)));
    page.on('console', message => { if (message.type() === 'error') errors.push(message.text()); });
    await page.goto(url);
    await page.waitForFunction(() => document.querySelector('#status')?.textContent !== 'Loading…', null, { timeout: 60000 });
    assert.equal(await page.locator('#status').textContent(), 'Ready');
    const canvas = page.locator('canvas');
    const input = page.locator('[data-sunfleck-editor]');
    await fs.mkdir(evidence, { recursive: true });
    // Exclude the focus border and caret, then compare actual glyph pixels.
    // Focused and blurred text must now use the identical core renderer.
    const compareFocus = async name => {
      await input.press('Control+End');
      await page.waitForTimeout(100);
      const box = await input.boundingBox();
      const clip = { x: box.x + 8, y: box.y + 4, width: Math.min(40, box.width - 16), height: box.height - 8 };
      const focused = await page.screenshot({ clip, path: path.join(evidence, `${name}-focused.png`) });
      await page.locator('#status').click();
      await input.waitFor({ state: 'detached' });
      await page.waitForTimeout(100);
      const blurred = await page.screenshot({ clip, path: path.join(evidence, `${name}-blurred.png`) });
      const extents = await page.evaluate(async images => {
        return Promise.all(images.map(async encoded => {
          const bytes = Uint8Array.from(atob(encoded), c => c.charCodeAt(0));
          const image = await createImageBitmap(new Blob([bytes], { type: 'image/png' }));
          const surface = document.createElement('canvas'); surface.width = image.width; surface.height = image.height;
          const context = surface.getContext('2d'); context.drawImage(image, 0, 0); image.close();
          const { data, width, height } = context.getImageData(0, 0, surface.width, surface.height);
          let left = width, top = height, right = -1, bottom = -1;
          for (let y = 0; y < height; ++y) for (let x = 0; x < width; ++x) {
            const i = (y * width + x) * 4;
            if (Math.max(data[i], data[i + 1], data[i + 2]) < 130) {
              left = Math.min(left, x); right = Math.max(right, x); top = Math.min(top, y); bottom = Math.max(bottom, y);
            }
          }
          return { bounds: [left, top, right, bottom], pixels: Array.from(data) };
        }));
      }, [focused.toString('base64'), blurred.toString('base64')]);
      assert.ok(extents.every(({ bounds: b }) => b[2] >= b[0]), `${name}: text must be visible in both states`);
      const changed = extents[0].pixels.reduce((count, value, i) => count + Number(value !== extents[1].pixels[i]), 0);
      assert.equal(changed, 0, `${name}: visible text pixels must match exactly after focus changes; positions ${extents[0].pixels.flatMap((v, i) => v !== extents[1].pixels[i] ? [Math.floor(i / 4)] : []).slice(0, 24)}`);
      const bounds = await canvas.boundingBox();
      await canvas.click({ position: { x: box.x - bounds.x + 20, y: box.y - bounds.y + 15 } });
      await input.waitFor();
    };
    const open = async () => { await canvas.click({ position: { x: 100, y: 266 } }); await input.waitFor(); };
    const reopen = async expected => {
      await canvas.click({ position: { x: 80, y: 170 } });
      assert.equal(await input.count(), 0);
      await open(); assert.equal(await input.inputValue(), expected);
    };
    await open();
    assert.equal(await input.inputValue(), 'Edit me');
    // Exercise the actual first key before any fill(), locator.press(), or
    // modifier key can refocus/wake the native editor and mask activation bugs.
    const firstSelection = await input.evaluate(e => [e.selectionStart, e.selectionEnd]);
    await page.keyboard.type('a');
    assert.equal(await input.evaluate(e => document.activeElement === e), true, 'first click must retain DOM editing focus');
    assert.equal(await input.inputValue(), 'Edit me'.slice(0, firstSelection[0]) + 'a' + 'Edit me'.slice(firstSelection[1]),
      'first plain key must be accepted without Shift or a second focus operation');
    await page.keyboard.press('Control+Z');
    assert.equal(await input.inputValue(), 'Edit me');
    // Safari may clear the DOM proxy on focus loss after restoring history
    // without dispatching an input event. Preserve the last committed text.
    await input.evaluate(element => { element.value = ''; });
    await page.locator('#status').click();
    await input.waitFor({ state: 'detached' });
    await open();
    assert.equal(await input.inputValue(), 'Edit me');
    const fontInfo = await input.evaluate(element => {
      const style = getComputedStyle(element);
      return {
        family: style.fontFamily,
        size: style.fontSize,
        loaded: document.fonts.check(`${style.fontSize} SunfleckFont`),
        registered: [...document.fonts].some(face => face.family === 'SunfleckFont' && face.status === 'loaded'),
      };
    });
    assert.match(fontInfo.family, /SunfleckFont/);
    assert.equal(fontInfo.loaded, true, 'focused editor must use the hosted core font');
    assert.equal(fontInfo.registered, true, 'font check must not pass through fallback alone');
    assert.equal(await input.evaluate(e => getComputedStyle(e).opacity), '0', 'DOM must remain focusable but paint no pixels');
    await compareFocus('single-line');
    // A correct UTF-8 round trip can still display tofu when an old/non-CJK
    // font is loaded. Assert rendered coverage as well as the committed value.
    await input.fill('让世界更美好');
    await reopen('让世界更美好');
    await input.press('End');
    await page.waitForTimeout(100);
    const textPixels = () => canvas.evaluate(e => {
      const scale = e.width / e.getBoundingClientRect().width;
      return Array.from((() => { const s = document.createElement('canvas'); s.width = e.width; s.height = e.height; const c = s.getContext('2d'); c.drawImage(e, 0, 0); return c; })().getImageData(40 * scale, 250 * scale, 180 * scale, 30 * scale).data).join(',');
    });
    const chinesePixels = await textPixels();
    await page.screenshot({ path: path.join(evidence, 'chinese-coverage.png') });
    await input.fill('□□□□□□');
    await page.waitForTimeout(100);
    assert.ok(await textPixels() !== chinesePixels, 'Chinese glyphs must not render as missing-glyph boxes');
    await input.fill('Edit me');
    await page.waitForTimeout(100);
    const beforeEdit = await canvas.evaluate(e => e.toDataURL());
    await input.fill('A🌱中文e\u0301');
    await page.waitForTimeout(100);
    assert.ok(await canvas.evaluate(e => e.toDataURL()) !== beforeEdit, 'committed edits must present a new core frame');
    await reopen('A🌱中文e\u0301');
    await input.press('Home'); await input.press('ArrowRight'); await input.press('Shift+ArrowRight');
    assert.deepEqual(await input.evaluate(e => [e.selectionStart, e.selectionEnd]), [1, 3]);
    await page.keyboard.insertText('X');
    await reopen('AX中文e\u0301');
    // Native browser copy/paste gestures, rather than a synthetic paste event.
    await input.press('Control+A'); await input.press('Control+C'); await input.press('End'); await input.press('Control+V');
    await reopen('AX中文e\u0301AX中文e\u0301');
    await input.fill('abcdef');
    await input.dblclick({ position: { x: 20, y: 16 } });
    assert.deepEqual(await input.evaluate(e => [e.selectionStart, e.selectionEnd]), [0, 6], 'double click selects the core-hit word');
    await input.press('Home'); await input.press('Shift+ArrowRight'); await input.press('Shift+ArrowRight');
    await page.waitForTimeout(100);
    const selectedFrame = await canvas.evaluate(e => e.toDataURL());
    await page.screenshot({ path: path.join(evidence, 'selection.png') });
    await input.press('ArrowRight'); await page.waitForTimeout(100);
    assert.notEqual(await canvas.evaluate(e => e.toDataURL()), selectedFrame, 'partial selection must be painted on Canvas');
    const dragBox = await input.boundingBox();
    await page.mouse.move(dragBox.x + 10, dragBox.y + 16); await page.mouse.down();
    await page.mouse.move(dragBox.x + 35, dragBox.y + 16, { steps: 5 }); await page.mouse.up();
    const dragged = await input.evaluate(e => [e.selectionStart, e.selectionEnd]);
    assert.equal(dragged[0], 0); assert.ok(dragged[1] > 0 && dragged[1] < 6, 'drag uses core character stops');
    await page.keyboard.insertText('Z');
    await reopen('Z' + 'abcdef'.slice(dragged[1]));
    await input.fill('');
    await reopen(''); // Start a fresh native undo transaction after test setup.
    await page.keyboard.type('undo');
    // Chromium may coalesce typing or split it at selection notifications.
    // Verify all native transactions can be undone and replayed without loss.
    let undoSteps = 0, remaining = 'undo';
    while (remaining && undoSteps < 4) {
      await input.press('Control+Z'); ++undoSteps;
      const next = await input.inputValue();
      assert.ok(next.length < remaining.length && remaining.startsWith(next));
      remaining = next;
    }
    assert.equal(remaining, '');
    for (let i = 0; i < undoSteps; ++i) await input.press('Control+Y');
    assert.equal(await input.inputValue(), 'undo');
    await input.fill('');
    await page.waitForTimeout(100);
    const modelPixels = () => canvas.evaluate(e => {
      const scale = e.width / e.getBoundingClientRect().width;
      const bytes = (() => { const s = document.createElement('canvas'); s.width = e.width; s.height = e.height; const c = s.getContext('2d'); c.drawImage(e, 0, 0); return c; })().getImageData(32 * scale, 400 * scale, 300 * scale, 32 * scale).data;
      return Array.from(bytes).join(',');
    });
    const beforeComposition = await canvas.evaluate(e => e.toDataURL());
    const beforeModel = await modelPixels();
    await input.evaluate(e => {
      window.compositionEvents = [];
      for (const type of ['compositionstart', 'compositionend']) e.addEventListener(type, () => window.compositionEvents.push(type));
    });
    const cdp = await page.context().newCDPSession(page);
    await cdp.send('Input.imeSetComposition', { text: 'zhong', selectionStart: 5, selectionEnd: 5 });
    await page.waitForTimeout(100);
    assert.ok(await canvas.evaluate(e => e.toDataURL()) !== beforeComposition, 'composition drafts must be drawn by the core');
    assert.equal(await modelPixels(), beforeModel, 'drafts must not publish to the bound text label');
    await page.screenshot({ path: path.join(evidence, 'composition.png') });
    await cdp.send('Input.insertText', { text: '中文🌱' });
    await page.waitForTimeout(100);
    assert.deepEqual(await page.evaluate(() => window.compositionEvents), ['compositionstart', 'compositionend']);
    await reopen('中文🌱');
    await input.press('End');
    const beforeCancel = await modelPixels();
    await cdp.send('Input.imeSetComposition', { text: 'cancel', selectionStart: 6, selectionEnd: 6 });
    await page.waitForTimeout(100);
    await cdp.send('Input.imeSetComposition', { text: '', selectionStart: 0, selectionEnd: 0 });
    await page.waitForTimeout(100);
    assert.equal(await input.inputValue(), '中文🌱');
    assert.equal(await modelPixels(), beforeCancel, 'cancelled drafts must not alter the model');
    // Browser text selection survives a host resize and DPR-independent geometry.
    await input.evaluate(e => e.setSelectionRange(0, 2, 'backward'));
    await page.setViewportSize({ width: 800, height: 1000 });
    await page.waitForTimeout(100);
    assert.deepEqual(await input.evaluate(e => [e.selectionStart, e.selectionEnd, e.selectionDirection]), [0, 2, 'backward']);
    await input.press('Tab');
    assert.equal(await input.evaluate(e => e.tagName), 'TEXTAREA');
    await input.fill('第一行🌱\nsecond line');
    // Use glyphs covered by the bundled font for the exact visual comparison.
    await input.fill('第一行\nsecond line');
    await compareFocus('multiline');
    const longText = Array.from({ length: 20 }, (_, i) => `Line ${i} abcdef`).join('\n');
    await input.fill(longText); await input.press('Control+Home'); await page.waitForTimeout(100);
    const topFrame = await canvas.evaluate(e => e.toDataURL());
    await input.press('Control+End'); await page.waitForTimeout(100);
    assert.notEqual(await canvas.evaluate(e => e.toDataURL()), topFrame, 'core scroll reveals the final line');
    const scrollBefore = await input.evaluate(e => e.scrollTop);
    const wheelBox = await input.boundingBox();
    await page.mouse.move(wheelBox.x + 30, wheelBox.y + 20); await page.mouse.wheel(0, -100);
    await page.waitForTimeout(100);
    assert.ok(await input.evaluate(e => e.scrollTop) < scrollBefore, 'wheel scroll is synchronized from the core');
    await input.fill('第一行🌱\nsecond line');
    await input.press('Tab');
    assert.equal(await input.getAttribute('type'), 'password');
    await input.fill('secret🌱');
    await input.press('Shift+Tab');
    assert.equal(await input.inputValue(), '第一行🌱\nsecond line');
    await input.press('Shift+Tab');
    assert.equal(await input.inputValue(), '中文🌱');
    await input.fill('\u05d0\u05d1\u05d2');
    const bidi = await input.evaluate(element => ({
      padding: parseFloat(getComputedStyle(element).paddingLeft),
      size: parseFloat(getComputedStyle(element).fontSize),
    }));
    const bidiBox = await input.boundingBox();
    const hitCaret = async distance => {
      await page.mouse.click(bidiBox.x + bidi.padding + distance, bidiBox.y + bidiBox.height / 2);
      return input.evaluate(element => element.selectionStart);
    };
    const leftCaret = await hitCaret(2);
    const rightCaret = await hitCaret(bidi.size * 3);
    assert.ok(leftCaret > rightCaret, `Hebrew hit testing follows visual RTL order (${leftCaret} vs ${rightCaret})`);
    await fs.mkdir(evidence, { recursive: true });
    await page.screenshot({ path: path.join(evidence, 'editing.png') });
    // Dispose while the browser still owns a composition draft.
    await cdp.send('Input.imeSetComposition', { text: 'pending', selectionStart: 7, selectionEnd: 7 });
    await page.locator('#stop').click();
    assert.equal(await page.locator('#status').textContent(), 'Closed');
    assert.equal(await input.count(), 0);
    await page.waitForTimeout(100);
    assert.deepEqual(errors, []);
    const result = { url, browser: browser.version(), dpr, result: 'passed', ime: 'CDP composition; OS candidate UI not tested' };
    await fs.writeFile(path.join(evidence, 'result.json'), JSON.stringify(result, null, 2));
    console.log(JSON.stringify(result));
  } catch (error) { console.error('Browser errors:', errors); throw error; }
  finally { await browser.close(); }
})().catch(error => { console.error(error); process.exitCode = 1; });

