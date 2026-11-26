param(
    [Parameter(Mandatory)][string]$Font,
    [Parameter(Mandatory)][string]$EmsdkRoot,
    [Parameter(Mandatory)][string]$Dotnet,
    [string]$Python = 'python',
    [string]$OutputDirectory,
    [string]$NativeBuildDirectory
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (!$OutputDirectory) { $OutputDirectory = Join-Path $root ('out/web-candidates/' + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
$output = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $output) { throw 'Candidate directory already exists. Choose a new directory.' }
if (!$NativeBuildDirectory) { $NativeBuildDirectory = Join-Path $root 'out/web-native' }
$native = (Resolve-Path -LiteralPath $NativeBuildDirectory).Path
$compiler = (Get-Command cl.exe -ErrorAction Stop).Source
function Invoke-Logged([string]$Program, [string[]]$Arguments, [string]$Log) {
    & $Program @Arguments *> $Log
    if ($LASTEXITCODE) { throw "$Program failed; see $Log" }
}
Push-Location $root
try {
    # Refuse to change an arbitrary caller's native configuration.
    if (!(Select-String -LiteralPath "$native/CMakeCache.txt" -Pattern '^UI_BUILD_CSHARP:BOOL=OFF$' -Quiet) -or
        !(Select-String -LiteralPath "$native/CMakeCache.txt" -Pattern '^UI_ENABLE_D3D11:BOOL=OFF$' -Quiet)) {
        throw 'Use the existing native verification configuration (C# OFF, D3D11 OFF).'
    }
    $buildLogs = Join-Path $root ('out/web-candidate-build-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $buildLogs | Out-Null
    Invoke-Logged $Python @("$PSScriptRoot/api-inventory.py", '--check') "$buildLogs/api-inventory.log"
    Invoke-Logged $Python @("$root/tests/web/tooling.py") "$buildLogs/tooling.log"
    $sourceBefore = & $Python "$PSScriptRoot/release.py" fingerprint
    if ($LASTEXITCODE) { throw 'Cannot fingerprint source inputs.' }
    Write-Host 'Building both browser hosts...'
    & "$PSScriptRoot/build.ps1" -Target Cpp -Font $Font -EmsdkRoot $EmsdkRoot -BuildDirectory "$root/out/web-cpp-text" *> "$buildLogs/cpp.log"
    & "$PSScriptRoot/build.ps1" -Target Dotnet -Font $Font -EmsdkRoot $EmsdkRoot -Dotnet $Dotnet -Publish *> "$buildLogs/dotnet.log"
    $sourceAfter = & $Python "$PSScriptRoot/release.py" fingerprint
    if ($LASTEXITCODE -or $sourceBefore -ne $sourceAfter) { throw 'Source changed during the build. Create a new candidate.' }
    Invoke-Logged $Python @("$PSScriptRoot/release.py", 'freeze', $output, '--cpp', "$root/out/web-cpp-text/web", '--dotnet', "$root/web/dotnet/bin/Release/net10.0/publish/wwwroot") "$buildLogs/freeze.log"
    Copy-Item -LiteralPath $buildLogs -Destination "$output/build-logs" -Recurse
    $environment = [ordered]@{
        dotnetSdk = (& $Dotnet --version | Out-String).Trim()
        emscripten = (& "$EmsdkRoot/upstream/emscripten/emcc.bat" --version | Out-String).Trim()
        node = (& node --version | Out-String).Trim()
        python = (& $Python --version | Out-String).Trim()
        cmake = (& cmake --version | Out-String).Trim()
        nativeCompiler = $compiler
        nativeCompilerVersion = (Get-Item -LiteralPath $compiler).VersionInfo.FileVersion
        nativeConfiguration = 'Debug; D3D11 OFF; noninteractive'
        font = (Resolve-Path -LiteralPath $Font).Path
    }
    $environment | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath "$output/environment.json" -Encoding utf8
    Invoke-Logged $Python @("$PSScriptRoot/font-report.py", $Font, '--output', "$output/font-report.json") "$output/font.log"
    Write-Host 'Running native and desktop C# regressions...'
    Invoke-Logged cmake @('--build', $native) "$output/native-build.log"
    Invoke-Logged ctest @('--test-dir', $native, '-LE', 'interactive', '--output-on-failure', '--output-junit', "$output/native.xml") "$output/native.log"
    try {
        Invoke-Logged cmake @('-S', $root, '-B', $native, '-DUI_BUILD_CSHARP=ON', "-DDOTNET_EXECUTABLE=$Dotnet") "$output/csharp-configure.log"
        Invoke-Logged cmake @('--build', $native, '--target', 'refresh_state_csharp') "$output/csharp-build.log"
        Invoke-Logged ctest @('--test-dir', $native, '-R', '^refresh_state_csharp$', '--output-on-failure', '--output-junit', "$output/csharp.xml") "$output/csharp.log"
    } finally {
        Invoke-Logged cmake @('-S', $root, '-B', $native, '-DUI_BUILD_CSHARP=OFF') "$output/native-restore.log"
    }
    Write-Host 'Running all 20 browser scenarios against the frozen candidate...'
    & "$PSScriptRoot/verify.ps1" -SkipBuild -Complete -Font $Font -EmsdkRoot $EmsdkRoot -Dotnet $Dotnet -Python $Python `
        -CppDirectory "$output/cpp" -DotnetDirectory "$output/dotnet" -EvidenceDirectory "$output/evidence" *> "$output/browser.log"
    # Keep the seal process's open stdout file outside the sealed inventory.
    Invoke-Logged $Python @("$PSScriptRoot/release.py", 'seal', $output) "$buildLogs/seal.log"
    Write-Host "Validated local candidate: $output"
} finally { Pop-Location }
