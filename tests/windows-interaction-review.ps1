param([string]$Directory = $PSScriptRoot, [switch]$Retest, [switch]$FocusOnly)
$ErrorActionPreference = 'Stop'
$fixture = Join-Path $Directory 'windows_accessibility_fixture.exe'
$cases = [ordered]@{
    narrator = '打开讲述人，按 Tab 依次访问按钮、复选框、文本框、密码框、滑块和组合框。核对名称、角色、值和焦点播报，并确认密码内容保密。'
    keyboard = '在普通主题下，用 Tab 和 Shift+Tab 将焦点移入、移出 Apply 按钮。检查能否一眼辨认当前按钮，再用方向键调节滑块。'
    contrast = '保持窗口打开，切换到 Windows 对比度主题。用 Tab 在按钮间移动，检查能否清楚识别焦点、读清文字。恢复原主题并再次确认。'
    textDrop = '把 Apply 按钮拖到文本框，确认文本变为 dropped text。'
    filesDrop = '将 Drag two paths 拖到 Drop here，确认目标文字变为 two paths。'
    objectDrop = '将 Drag object 拖到同一目标，确认文字变为 object 42。'
    dpi = '分别在 100%、150%、200% 显示缩放下检查焦点、辅助功能命中位置和拖放，然后恢复原来的缩放设置。'
}
if ($FocusOnly) {
    foreach ($key in @($cases.Keys)) { if ($key -notin 'keyboard','contrast') { $cases.Remove($key) } }
} elseif ($Retest) {
    foreach ($key in 'narrator','textDrop','dpi') { $cases.Remove($key) }
}
$outcomes = @{'通过'='pass'; '失败'='fail'; '跳过'='skip'; '1'='pass'; '2'='fail'; '3'='skip'; pass='pass'; fail='fail'; skip='skip'}
$checks = [ordered]@{}
$process = Start-Process -FilePath $fixture -PassThru
try {
    foreach ($entry in $cases.GetEnumerator()) {
        Write-Host $entry.Value
        do {
            $answer = (Read-Host '请输入结果（1 通过，2 失败，3 跳过）').Trim()
        } until ($outcomes.ContainsKey($answer))
        $outcome = $outcomes[$answer]
        $checks[$entry.Key] = [ordered]@{outcome=$outcome; note=if ($outcome -ne 'pass') { Read-Host '请描述看到的现象或跳过的原因' } else { '' }}
    }
} finally {
    if (!$process.HasExited) { [void]$process.CloseMainWindow(); if (!$process.WaitForExit(5000)) { $process.Kill() } }
    $process.Dispose()
    $report = Join-Path $Directory 'interaction-result.json'
    [ordered]@{utc=[DateTime]::UtcNow.ToString('o'); os=[Environment]::OSVersion.VersionString
        sha256=(Get-FileHash $fixture).Hash; checks=$checks} | ConvertTo-Json -Depth 5 |
        Set-Content $report -Encoding utf8
    Write-Host "结果已保存到 $report"
}
if ($checks.Count -ne $cases.Count -or @($checks.Values | Where-Object { $_.outcome -ne 'pass' }).Count) { exit 1 }
