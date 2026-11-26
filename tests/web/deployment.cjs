const assert = require('node:assert/strict');
const path = require('node:path');
const fs = require('node:fs/promises');
const { createRequire } = require('node:module');
const dependencies = process.env.SUNFLECK_BROWSER_TEST_MODULES
  ? createRequire(path.resolve(process.env.SUNFLECK_BROWSER_TEST_MODULES, 'package.json')) : require;
const { chromium } = dependencies('playwright-core');
// Node 24's bundled undici can assert on a large gzip HTTP/1.0 response.
// Use the standard streaming HTTP client to verify the exact wire payload.
const get = url => new Promise((resolve, reject) => {
  require('node:http').get(url, { headers: { 'Accept-Encoding': 'gzip' } }, response => {
    const chunks = [];
    response.on('data', chunk => chunks.push(chunk));
    response.on('error', reject);
    response.on('end', () => {
      try {
        assert.equal(response.statusCode, 200);
        const wire = Buffer.concat(chunks);
        const data = response.headers['content-encoding'] === 'gzip' ? require('node:zlib').gunzipSync(wire) : wire;
        resolve({ headers: response.headers, data });
      } catch (error) { reject(error); }
    });
  }).on('error', reject);
});
(async () => {
  const url = process.argv[2]; const evidence = process.argv[3];
  const manifest = JSON.parse((await get(new URL('asset-manifest.json', url))).data.toString());
  const wasm = manifest.assets.find(a => a.path.endsWith('.wasm'));
  assert.ok(wasm);
  const response = await get(new URL(wasm.path, url));
  assert.equal(response.headers['content-type'], 'application/wasm');
  assert.equal(response.headers['content-encoding'], 'gzip');
  assert.equal(response.headers['cache-control'], 'no-cache');
  const data = response.data;
  assert.equal(require('node:crypto').createHash('sha256').update(data).digest('hex'), wasm.sha256);
  const browser = await chromium.launch({ channel: process.env.BROWSER_CHANNEL || 'msedge', headless: true });
  try {
    const page = await browser.newPage();
    await page.route('**/workspace.fleck', route => route.fulfill({ status: 404, body: 'Missing' }));
    await page.goto(new URL('?scenario=workspace', url).href);
    await page.waitForFunction(() => document.querySelector('#status').textContent.includes('Page request failed: 404'), null, { timeout: 60000 });
    await page.unroute('**/workspace.fleck');
    const fontPath = manifest.assets.some(a => a.path === 'sunfleck.data') ? '**/sunfleck.data' : '**/font.ttf';
    await page.route(fontPath, route => route.fulfill({ status: 404, body: 'Missing font' }));
    await page.goto(url);
    await page.waitForFunction(() => !['Ready', 'Loading…'].includes(document.querySelector('#status').textContent), null, { timeout: 60000 });
    const result = { result: 'passed', browser: browser.version(), url, checks: ['manifest SHA256', 'Wasm MIME', 'gzip', 'revalidation', 'missing page', 'missing font'] };
    await fs.mkdir(evidence, { recursive: true });
    await fs.writeFile(path.join(evidence, 'result.json'), JSON.stringify(result, null, 2));
    console.log(JSON.stringify(result));
  } finally { await browser.close(); }
})().catch(e => { console.error(e); process.exitCode = 1; });
