const { spawnSync } = require('node:child_process');
const path = require('node:path');
const fs = require('node:fs');
const cpp = process.env.SUNFLECK_CPP_URL || 'http://127.0.0.1:8767/';
const dotnet = process.env.SUNFLECK_DOTNET_URL || 'http://127.0.0.1:8766/';
const hosts = new Set((process.env.SUNFLECK_TEST_HOSTS || 'cpp,dotnet').split(','));
const cases = process.env.SUNFLECK_TEST_CASES ? new Set(process.env.SUNFLECK_TEST_CASES.split(',')) : null;
const matrix = [['smoke', [1]], ['text-input', [1, 2]], ['external-drop', [1]], ['workspace', [1, 1.25, 2]], ['window-state', [1]], ['renderer', [1]], ['deployment', [1]], ['performance', [1]]];
if ([...hosts].some(h => !['cpp', 'dotnet'].includes(h)) ||
    (cases && [...cases].some(c => !matrix.some(([name]) => name === c)))) throw new Error('Unknown test host or case');
const evidenceRoot = path.resolve(process.env.SUNFLECK_TEST_EVIDENCE || 'out/web-evidence');
fs.mkdirSync(evidenceRoot, { recursive: true });
const summary = { startedUtc: new Date().toISOString(), result: 'running', cases: [] };
const save = () => fs.writeFileSync(path.join(evidenceRoot, 'suite-result.json'), JSON.stringify(summary, null, 2));
save();
for (const [host, url] of [['cpp', cpp], ['dotnet', dotnet]]) {
  if (!hosts.has(host)) continue;
  for (const [test, dprs] of matrix) {
    if (cases && !cases.has(test)) continue;
    for (const dpr of dprs) {
      const name = `complete-${host}-${test}-${dpr}`;
      const result = spawnSync(process.execPath, [path.join(__dirname, `${test}.cjs`), url,
        path.join(evidenceRoot, name)], {
        encoding: 'utf8', maxBuffer: 32 * 1024 * 1024, env: { ...process.env, SUNFLECK_TEST_DPR: String(dpr) },
      });
      fs.writeFileSync(path.join(evidenceRoot, `${name}.log`), (result.stdout || '') + (result.stderr || '') + (result.error ? String(result.error) : ''));
      process.stdout.write(result.stdout || ''); process.stderr.write(result.stderr || '');
      summary.cases.push({ host, test, dpr, result: result.status === 0 ? 'passed' : 'failed', exitCode: result.status });
      if (result.status !== 0) { summary.result = 'failed'; save(); process.exit(result.status || 1); }
      save();
    }
  }
}
summary.result = 'passed'; summary.finishedUtc = new Date().toISOString(); save();
