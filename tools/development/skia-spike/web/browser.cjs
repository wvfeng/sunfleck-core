const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const path = require('node:path');
const { createRequire } = require('node:module');
const dependencies = process.env.SUNFLECK_BROWSER_TEST_MODULES
  ? createRequire(path.resolve(process.env.SUNFLECK_BROWSER_TEST_MODULES, 'package.json'))
  : createRequire(path.resolve(__dirname, '../../../../tests/web/package.json'));
const { chromium } = dependencies('playwright-core');

(async () => {
  const [url, evidence] = process.argv.slice(2);
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  const errors = [], results = [], featureComparisons = [];
  let currentPage, currentDpi;
  try {
    await fs.mkdir(evidence, { recursive: true });
    for (const dpi of [1, 1.5, 2]) {
      const page = await browser.newPage({ viewport: { width: 1000, height: 700 }, deviceScaleFactor: dpi });
      currentPage=page;currentDpi=dpi;
      page.on('pageerror', e => errors.push(String(e)));
      page.on('console', e => {
        if(e.type() === 'error') errors.push(e.text());
        if(e.text().startsWith('comparison dpi=')) featureComparisons.push(e.text());
      });
      await page.goto(url);
      await page.waitForFunction(() => document.querySelector('#status')?.textContent !== 'Loading…', null, { timeout: 90000 });
      assert.equal(await page.locator('#status').textContent(), 'Ready');
      await page.waitForSelector('#surface[data-renderer="skia-webgl2"][data-renderer-state="ready"]');
      await page.waitForFunction(() => globalThis.skiaEvaluationLastCommands?.length > 0);
      const comparison = await page.evaluate(async () => {
        const canvas = document.querySelector('#surface');
        const reference = document.createElement('canvas'); reference.width=canvas.width;reference.height=canvas.height;
        const { createRenderer } = await import('./reference-renderer.js');
        const renderer = createRenderer(reference, 'webgl2');
        renderer.present(globalThis.skiaEvaluationLastCommands);
        const read = c => {
          const copy=document.createElement('canvas');copy.width=c.width;copy.height=c.height;
          const ctx=copy.getContext('2d');ctx.drawImage(c,0,0);return ctx.getImageData(0,0,c.width,c.height).data;
        };
        const a=read(canvas),b=read(reference);let max=0,total=0,large=0;
        for(let i=0;i<a.length;i++){const d=Math.abs(a[i]-b[i]);max=Math.max(max,d);total+=d;large+=d>3;}
        const gl=canvas.getContext('webgl2'),debug=gl.getExtension('WEBGL_debug_renderer_info');
        const result={ max,mean:total/a.length,fractionAbove3:large/a.length,rectangles:globalThis.skiaEvaluationLastCommands.length/20,
          fontVersion:globalThis.skiaEvaluationBridge.fontVersion(),
          renderer:debug?gl.getParameter(debug.UNMASKED_RENDERER_WEBGL):gl.getParameter(gl.RENDERER) };
        renderer.close();return result;
      });
      assert.ok(comparison.max<=3, `Skia/production command rendering difference: ${JSON.stringify(comparison)}`);
      assert.equal(comparison.fontVersion,21500,'Both native renderers must resolve the pinned FreeType runtime');
      if(dpi===1)assert.equal(await page.evaluate(()=>skiaEvaluationBridge.features()),0,'Native Web CPU/GPU effects and font transfer');
      const canvas=page.locator('#surface');
      const clickControl=async name => {
        const box=await page.locator(`[data-node-id][aria-label="${name}"]`).boundingBox();
        assert.ok(box,`Control bounds: ${name}`);await page.mouse.click(box.x+box.width/2,box.y+box.height/2);
      };
      const countIs=async text => page.waitForFunction(value=>[...document.querySelectorAll('[data-node-id][role="status"]')].some(n=>n.textContent===value),text);
      const initial=await canvas.evaluate(c=>c.toDataURL());
      await clickControl('Increment');await countIs('Count: 1');await page.mouse.move(5,5);
      await page.evaluate(()=>new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve))));
      assert.notEqual(await canvas.evaluate(c=>c.toDataURL()),initial,'Pointer command must change rendered pixels');
      await clickControl('Increment after 500 ms');await clickControl('Increment after 500 ms');
      await clickControl('Increment');await countIs('Count: 3');
      await clickControl('editor');
      const input=page.locator('[data-sunfleck-editor]');await input.waitFor();
      await input.fill('A🌱中文e\u0301');
      await page.locator('#status').click();
      await clickControl('editor');
      await input.waitFor();assert.equal(await input.inputValue(),'A🌱中文e\u0301');
      await page.locator('#status').click();
      await page.waitForTimeout(100);
      const before=await canvas.evaluate(c=>c.toDataURL());
      await page.evaluate(() => {
        globalThis.skiaLoss=document.querySelector('#surface').getContext('webgl2').getExtension('WEBGL_lose_context');
        if(!skiaLoss)throw new Error('Context loss test extension unavailable');skiaLoss.loseContext();
      });
      await page.waitForSelector('#surface[data-renderer-state="lost"]');
      await page.evaluate(()=>skiaLoss.restoreContext());
      await page.waitForSelector('#surface[data-renderer-state="ready"]');
      assert.equal(await canvas.evaluate(c=>c.toDataURL()),before,'Restored idle frame must match');
      await page.setViewportSize({width:700,height:600});
      await page.waitForFunction(()=>document.querySelector('#surface').width===Math.round(document.querySelector('#surface').getBoundingClientRect().width*devicePixelRatio));
      await page.screenshot({path:path.join(evidence,`dpr-${dpi}.png`)});
      await page.locator('#stop').click();assert.equal(await canvas.getAttribute('data-renderer-state'),'closed');
      results.push({dpi,...comparison});await page.close();
    }
    assert.deepEqual(errors,[]);
    const result={url,browser:browser.version(),result:'passed',results,featureComparisons};
    await fs.writeFile(path.join(evidence,'result.json'),JSON.stringify(result,null,2));console.log(JSON.stringify(result));
  } catch (error) {
    await fs.mkdir(evidence, { recursive: true });
    if(currentPage && !currentPage.isClosed()) {
      await currentPage.screenshot({path:path.join(evidence,'failure.png')}).catch(()=>{});
    }
    await fs.writeFile(path.join(evidence,'result.json'),JSON.stringify({url,browser:browser.version(),
      result:'failed',dpi:currentDpi,error:String(error),errors,results,featureComparisons},null,2));
    throw error;
  } finally { await browser.close(); }
})().catch(error=>{console.error(error);process.exitCode=1;});
