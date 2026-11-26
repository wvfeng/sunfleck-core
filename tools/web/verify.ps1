param(
    [Parameter(Mandatory)][string]$Font,
    [Parameter(Mandatory)][string]$EmsdkRoot,
    [string]$Dotnet = 'dotnet',
    [string]$Python = 'python',
    [int]$CppPort = 8768,
    [int]$DotnetPort = 8769,
    [string]$CppDirectory,
    [string]$DotnetDirectory,
    [string]$EvidenceDirectory,
    [switch]$Complete,
    [switch]$SkipBuild
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$cpp = Join-Path $root 'out/web-cpp-text/web'
$managed = Join-Path $root 'web/dotnet/bin/Release/net10.0/publish/wwwroot'
if ($CppDirectory) { $cpp = (Resolve-Path -LiteralPath $CppDirectory).Path }
if ($DotnetDirectory) { $managed = (Resolve-Path -LiteralPath $DotnetDirectory).Path }
if (($CppDirectory -or $DotnetDirectory) -and !$SkipBuild) { throw 'Custom site directories require -SkipBuild.' }
$processes = @()
$savedCpp = $env:SUNFLECK_CPP_URL
$savedDotnet = $env:SUNFLECK_DOTNET_URL
$savedEvidence = $env:SUNFLECK_TEST_EVIDENCE
$savedHosts = $env:SUNFLECK_TEST_HOSTS
$savedCases = $env:SUNFLECK_TEST_CASES
Push-Location $root
try {
    if ($EvidenceDirectory) { $env:SUNFLECK_TEST_EVIDENCE = [IO.Path]::GetFullPath($EvidenceDirectory) }
    if ($Complete) { $env:SUNFLECK_TEST_HOSTS = 'cpp,dotnet'; $env:SUNFLECK_TEST_CASES = $null }
    if (!$SkipBuild) {
        & "$PSScriptRoot/build.ps1" -Target Cpp -Font $Font -EmsdkRoot $EmsdkRoot -BuildDirectory "$root/out/web-cpp-text"
        & "$PSScriptRoot/build.ps1" -Target Dotnet -Font $Font -EmsdkRoot $EmsdkRoot -Dotnet $Dotnet -Publish
    }
    foreach ($site in @($cpp, $managed)) {
        & $Python "$PSScriptRoot/serve.py" $site --package
        if ($LASTEXITCODE) { throw 'Packaging failed.' }
    }
    foreach ($spec in @(@($cpp, $CppPort), @($managed, $DotnetPort))) {
        # Start-Process argument strings need explicit quotes for paths with spaces.
        $arguments = @('"' + "$PSScriptRoot/serve.py" + '"', '"' + $spec[0] + '"', '--port', $spec[1])
        $processes += Start-Process -FilePath $Python -ArgumentList $arguments -WindowStyle Hidden -PassThru
    }
    $env:SUNFLECK_CPP_URL = "http://127.0.0.1:$CppPort/"
    $env:SUNFLECK_DOTNET_URL = "http://127.0.0.1:$DotnetPort/"
    foreach ($url in @($env:SUNFLECK_CPP_URL, $env:SUNFLECK_DOTNET_URL)) {
        $ready = $false
        for ($attempt = 0; $attempt -lt 30; $attempt++) {
            try { Invoke-WebRequest $url -UseBasicParsing -TimeoutSec 2 | Out-Null; $ready = $true; break }
            catch { Start-Sleep -Milliseconds 200 }
        }
        if (!$ready) { throw "Preview failed to start: $url" }
    }
    & node tests/web/run.cjs
    if ($LASTEXITCODE) { throw 'Web regression failed.' }
} finally {
    foreach ($process in $processes) { if (!$process.HasExited) { Stop-Process -Id $process.Id } }
    $env:SUNFLECK_CPP_URL = $savedCpp
    $env:SUNFLECK_DOTNET_URL = $savedDotnet
    $env:SUNFLECK_TEST_EVIDENCE = $savedEvidence
    $env:SUNFLECK_TEST_HOSTS = $savedHosts
    $env:SUNFLECK_TEST_CASES = $savedCases
    Pop-Location
}
