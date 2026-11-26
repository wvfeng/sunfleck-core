const assert = require('node:assert/strict');
const path = require('node:path');
const fs = require('node:fs/promises');
const { createRequire } = require('node:module');
const dependencies = process.env.SUNFLECK_BROWSER_TEST_MODULES
  ? createRequire(path.resolve(process.env.SUNFLECK_BROWSER_TEST_MODULES, 'package.json')) : require;
const { chromium } = dependencies('playwright-core');

(async () => {
  const url = new URL(process.argv[2] || 'http://127.0.0.1:8767/'); url.searchParams.set('scenario', 'workspace');
  url.searchParams.set('verify', '1');
  const evidence = process.argv[3] || 'out/web-evidence/workspace';
  const dpr = Number(process.env.SUNFLECK_TEST_DPR || 1);
  const browser = await chromium.launch({ channel: process.env.BROWSER_CHANNEL || 'msedge', headless: true });
  const errors = [];
  try {
    const page = await browser.newPage({ viewport: { width: 1000, height: 1000 }, deviceScaleFactor: dpr });
    page.on('pageerror', e => errors.push(String(e)));
    await page.goto(url.href);
    await page.waitForFunction(() => document.querySelector('#status').textContent !== 'Loading…', null, { timeout: 60000 });
    assert.equal(await page.locator('#status').textContent(), 'Ready');
    assert.equal(await page.locator('body').getAttribute('data-contracts'), 'passed');
    const semantic = name => page.locator(`button[data-node-id][aria-label="${name}"]`);
    // Accessibility activation uses the same core actions as pointer/keyboard.
    const activate = async name => { await semantic(name).evaluate(e => e.click()); await page.waitForTimeout(150); };
    await page.waitForFunction(() => document.querySelector('button[aria-label="search"]'));
    const focusedName = name => page.waitForFunction(label => document.activeElement?.getAttribute('aria-label') === label, name);
    await page.locator('#surface').focus();
    await page.keyboard.press('Tab');
    await focusedName('Float document');
    const networkStatus = status => page.waitForFunction(s => document.querySelector(`[role="status"][aria-label="Network: ${s}"]`), status);
    await activate('Load message'); await networkStatus('loaded');
    await page.route('**/message.txt', route => route.fulfill({ status: 503, body: 'Unavailable' }));
    await activate('Load message'); await networkStatus('failed');
    await page.unroute('**/message.txt');
    await page.route('**/message.txt', async route => { await new Promise(r => setTimeout(r, 3000)); await route.fulfill({ status: 200, body: 'Late' }).catch(() => {}); });
    await activate('Load message'); await networkStatus('timeout');
    await activate('Load message'); await activate('Cancel request'); await networkStatus('cancelled');
    await page.unroute('**/message.txt');
    await activate('search');
    const editor = page.locator('[data-sunfleck-editor]');
    await focusedName('search');
    await editor.fill('中文搜索');
    await editor.press('Tab');
    await focusedName('Enable option');
    await page.keyboard.press('Space');
    await page.waitForFunction(() => document.activeElement?.getAttribute('aria-checked') === 'true');
    await activate('search');
    assert.equal(await editor.inputValue(), '中文搜索');
    await activate('Enable option');
    assert.equal(await semantic('Enable option').getAttribute('aria-checked'), 'false');
    const cdp = await page.context().newCDPSession(page);
    const ax = await cdp.send('Accessibility.getFullAXTree');
    assert.ok(ax.nodes.some(n => n.role?.value === 'checkbox' && n.name?.value === 'Enable option' &&
      n.properties?.some(p => p.name === 'focused' && p.value.value === true)));
    assert.ok(ax.nodes.some(n => n.role?.value === 'button' && n.name?.value === 'Float document'));
    // Scroll a non-editor container with actual mouse wheel events.
    const canvas = page.locator('#surface');
    const box = await canvas.boundingBox();
    await page.mouse.move(box.x + 110, box.y + 300);
    const before = await canvas.evaluate(c => c.toDataURL());
    await page.mouse.wheel(0, 1400); await page.waitForTimeout(200);
    assert.notEqual(await canvas.evaluate(c => c.toDataURL()), before);
    assert.ok(await semantic('Scroll marker 10').count());
    await activate('Float document');
    await page.waitForSelector('canvas[data-view-id]');
    const floating = page.locator('canvas[data-view-id]');
    assert.equal(await page.locator('#status').textContent(), 'Ready');
    const origin = await floating.boundingBox();
    await page.mouse.move(origin.x + 80, origin.y + 16); await page.mouse.down();
    await page.mouse.move(origin.x + 160, origin.y + 56, { steps: 8 }); await page.mouse.up();
    await page.waitForTimeout(150);
    const moved = await floating.boundingBox();
    assert.ok(moved.x > origin.x + 40 && moved.y > origin.y + 20, `floating caption moves within the page: ${JSON.stringify({ origin, moved })}`);
    await page.mouse.move(moved.x + moved.width - 3, moved.y + moved.height - 3); await page.mouse.down();
    await page.mouse.move(moved.x + moved.width + 57, moved.y + moved.height + 37, { steps: 8 }); await page.mouse.up();
    await page.waitForTimeout(150);
    const resized = await floating.boundingBox();
    assert.ok(resized.width > moved.width + 30 && resized.height > moved.height + 20, 'floating corner resizes its core surface');
    await activate('documentText');
    await editor.fill('浮动中文编辑 😀');
    assert.equal(await editor.inputValue(), '浮动中文编辑 😀');
    const dockFrom = await floating.boundingBox();
    const dockTo = await canvas.boundingBox();
    await page.mouse.move(dockFrom.x + 70, dockFrom.y + 55); await page.mouse.down();
    await page.mouse.move(dockTo.x + dockTo.width - 50, dockTo.y + dockTo.height / 2, { steps: 12 });
    await page.mouse.up();
    await page.waitForFunction(() => !document.querySelector('canvas[data-view-id]'));
    await activate('Reset workspace');
    await page.waitForFunction(() => !document.querySelector('canvas[data-view-id]'));
    await activate('documentText');
    assert.equal(await editor.inputValue(), '浮动中文编辑 😀');
    // Explicit persisted lifecycle coverage, followed by real navigation/back.
    await page.evaluate(() => window.dispatchEvent(new PageTransitionEvent('pagehide', { persisted: true })));
    assert.equal(await page.locator('#status').textContent(), 'Ready');
    await page.evaluate(() => window.dispatchEvent(new PageTransitionEvent('pageshow', { persisted: true })));
    await activate('Float document');
    await page.waitForSelector('canvas[data-view-id]');
    await page.goto(new URL('index.html', url).href);
    await page.goBack();
    await page.waitForFunction(() => document.querySelector('#status')?.textContent === 'Ready');
    await page.waitForFunction(() => document.querySelector('button[aria-label="Float document"]'));
    await activate('Reset workspace');
    await activate('Float document');
    await page.waitForSelector('canvas[data-view-id]');
    await fs.mkdir(evidence, { recursive: true });
    await page.screenshot({ path: path.join(evidence, 'workspace.png') });
    const closeBox = await floating.boundingBox();
    await page.mouse.click(closeBox.x + closeBox.width - 22, closeBox.y + 16);
    await page.waitForFunction(() => !document.querySelector('canvas[data-view-id]'));
    assert.equal(await page.locator('#status').textContent(), 'Ready');
    const performance = await page.evaluate(async () => {
      const deltas = []; let last = performance.now();
      for (let i = 0; i < 90; ++i) await new Promise(resolve => requestAnimationFrame(now => { deltas.push(now-last); last=now; resolve(); }));
      deltas.sort((a,b) => a-b);
      return { frameMedianMs: deltas[45], frameP95Ms: deltas[85],
        jsHeapBytes: performance.memory?.usedJSHeapSize ?? null,
        resources: performance.getEntriesByType('resource').map(r => ({ name: new URL(r.name).pathname, bytes: r.transferSize, durationMs: r.duration })) };
    });
    await page.route('**/message.txt', async route => { await new Promise(r => setTimeout(r, 3000)); await route.fulfill({ status: 200, body: 'Late' }).catch(() => {}); });
    await activate('Load message');
    await page.locator('#stop').click();
    await page.waitForTimeout(2200);
    assert.equal(await page.locator('canvas[data-view-id]').count(), 0);
    assert.equal(await editor.count(), 0);
    assert.deepEqual(errors, []);
    const result = { url: url.href, browser: browser.version(), dpr, result: 'passed', performance,
      lifecycle: 'persisted events and real navigation/back; actual BFCache eligibility is browser-controlled' };
    await fs.writeFile(path.join(evidence, 'result.json'), JSON.stringify(result, null, 2));
    console.log(JSON.stringify(result));
  } catch(e) { console.error(errors); throw e; }
  finally { await browser.close(); }
})().catch(e => { console.error(e); process.exitCode = 1; });
