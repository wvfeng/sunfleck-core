param(
    [Parameter(Mandatory)][string]$EmsdkRoot,
    [string]$BuildDirectory,
    [ValidateRange(1, 64)][int]$Jobs = 2
)
$ErrorActionPreference = 'Stop'
# Emscripten builds system libraries internally, independently of Ninja's -j.
$env:EMCC_CORES = [string]$Jobs
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$emsdk = (Resolve-Path -LiteralPath $EmsdkRoot).Path
if (!$BuildDirectory) { $BuildDirectory = Join-Path $root 'out/web-core-tests' }
$output = [IO.Path]::GetFullPath($BuildDirectory)
function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed ($LASTEXITCODE)." }
}
Invoke-Checked cmake @('-S', $root, '-B', $output, '-G', 'Ninja',
    "-DCMAKE_TOOLCHAIN_FILE=$emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake",
    '-DCMAKE_BUILD_TYPE=Release', '-DBUILD_TESTING=OFF', '-DUI_BUILD_TOOLS=OFF',
    '-DUI_BUILD_EXAMPLES=OFF', '-DUI_BUILD_DEMOS=OFF', '-DUI_BUILD_CSHARP=OFF', '-DUI_BUILD_WEB=OFF')
Invoke-Checked cmake @('--build', $output, '-j', [string]$Jobs)
foreach ($name in @('value_literals', 'hosted_runtime', 'border', 'visual_state')) {
    Invoke-Checked "$emsdk/upstream/emscripten/em++.bat" @("$root/tests/$name.cpp", "$output/libsunfleck.a",
        "-I$root/include", '-std=c++20', '-O2', '-fexceptions', '-sUSE_FREETYPE=1',
        '-sENVIRONMENT=node', '-sWASM_ASYNC_COMPILATION=0', '-sALLOW_MEMORY_GROWTH=1',
        '-sFILESYSTEM=0', '-o', "$output/$name.js")
    Invoke-Checked node @("$output/$name.js")
    Write-Host "$name passed (single-threaded Wasm, no filesystem)."
}
Write-Host 'Core checks only: this does not validate browser input, rendering, accessibility or Linux.'
