param(
    [ValidateSet('Cpp', 'Dotnet')][string]$Target = 'Cpp',
    [Parameter(Mandatory)][string]$Font,
    [string]$EmsdkRoot,
    [string]$Dotnet = 'dotnet',
    [string]$BuildDirectory,
    [string]$CppProject,
    [string]$DotnetProject,
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [switch]$Publish
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$fontPath = (Resolve-Path -LiteralPath $Font).Path
if ($BuildDirectory) { $BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory) }
function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed ($LASTEXITCODE)." }
}
$names = @('EMSDK_PYTHON', 'PYTHONUTF8', 'DOTNET_EMSCRIPTEN_LLVM_ROOT', 'DOTNET_EMSCRIPTEN_BINARYEN_ROOT',
    'DOTNET_EMSCRIPTEN_NODE_JS', 'EM_CACHE', 'EM_FROZEN_CACHE', 'DOTNET_CLI_HOME', 'NUGET_PACKAGES')
$saved = @{}
foreach ($name in $names) { $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
Push-Location $root
try {
    $env:PYTHONUTF8 = '1'
    $common = @('-G', 'Ninja', '-DBUILD_TESTING=OFF', '-DUI_BUILD_TOOLS=OFF',
        '-DUI_BUILD_EXAMPLES=OFF', '-DUI_BUILD_DEMOS=OFF', '-DUI_BUILD_CSHARP=OFF')
    if ($Target -eq 'Cpp') {
        if (!$EmsdkRoot) { throw 'Specify -EmsdkRoot for an installed and activated Emscripten SDK.' }
        $emsdk = (Resolve-Path -LiteralPath $EmsdkRoot).Path
        # emcc.bat finds the SDK Python through its activated .emscripten config.
        $output = Join-Path $root 'out/web-cpp'
        if ($BuildDirectory) { $output = $BuildDirectory }
        $source = if ($CppProject) { (Resolve-Path -LiteralPath $CppProject).Path } else { $root }
        Invoke-Checked cmake (@('-S', $source, '-B', $output,
            "-DCMAKE_TOOLCHAIN_FILE=$emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake",
            '-DUI_BUILD_WEB=ON', "-DUI_WEB_FONT=$fontPath") + $common + "-DCMAKE_BUILD_TYPE=$Configuration")
        Invoke-Checked cmake @('--build', $output, '-j', '4')
        foreach ($obsolete in @('font.ttf', 'font.ttf.gz', 'font.ttf.br')) {
            $obsoleteFont = Join-Path $output "web/$obsolete"
            if (Test-Path -LiteralPath $obsoleteFont) { Remove-Item -LiteralPath $obsoleteFont }
        }
        Write-Host "Serve $output/web with an HTTP server."
    } else {
        $env:DOTNET_CLI_HOME = Join-Path $root 'out/web-dotnet-home'
        $env:NUGET_PACKAGES = Join-Path $root 'out/nuget'
        $project = Join-Path $root 'web/dotnet/Sunfleck.Web.csproj'
        if ($DotnetProject) { $project = (Resolve-Path -LiteralPath $DotnetProject).Path }
        $json = & $Dotnet msbuild $project '-getProperty:EmscriptenSdkToolsPath,EmscriptenPythonToolsPath,EmscriptenNodeToolsPath'
        if ($LASTEXITCODE -ne 0) { throw 'Install the .NET wasm-tools workload for the selected SDK first.' }
        $props = ($json | Out-String | ConvertFrom-Json).Properties
        if (!$props.EmscriptenSdkToolsPath -or !$props.EmscriptenPythonToolsPath -or !$props.EmscriptenNodeToolsPath) {
            throw 'The selected .NET SDK has no matching wasm-tools workload.'
        }
        $sdk = [IO.Path]::GetFullPath($props.EmscriptenSdkToolsPath)
        $toolchainProperties = @()
        $suffix = ''
        if ($EmsdkRoot) {
            $sdk = Join-Path (Resolve-Path -LiteralPath $EmsdkRoot).Path 'upstream'
            $suffix = '-emsdk'
            $toolchainProperties = @("-p:EmscriptenSdkToolsPath=$sdk/",
                "-p:EmscriptenUpstreamBinPath=$sdk/bin/",
                "-p:EmscriptenUpstreamEmscriptenPath=$sdk/emscripten/")
        }
        if (!$sdk -or !(Test-Path "$sdk/emscripten/emcc.py")) { throw 'The selected .NET SDK has no matching wasm-tools workload.' }
        $env:EMSDK_PYTHON = Join-Path $props.EmscriptenPythonToolsPath 'python.exe'
        $env:DOTNET_EMSCRIPTEN_LLVM_ROOT = Join-Path $sdk 'bin'
        $env:DOTNET_EMSCRIPTEN_BINARYEN_ROOT = $sdk
        $env:DOTNET_EMSCRIPTEN_NODE_JS = Join-Path $props.EmscriptenNodeToolsPath 'bin/node.exe'
        $env:EM_CACHE = Join-Path $root "out/web-dotnet-cache$suffix"
        $env:EM_FROZEN_CACHE = '0'
        $output = Join-Path $root "out/web-dotnet-native$suffix"
        if ($BuildDirectory) { $output = $BuildDirectory }
        Invoke-Checked cmake (@('-S', $root, '-B', $output,
            "-DCMAKE_TOOLCHAIN_FILE=$sdk/emscripten/cmake/Modules/Platform/Emscripten.cmake",
            '-DUI_WASM_NATIVE_EXCEPTIONS=ON', '-DCMAKE_BUILD_TYPE=Release') + $common)
        Invoke-Checked cmake @('--build', $output, '-j', '4')
        Copy-Item -LiteralPath "$output/libsunfleck.a" -Destination "$output/sunfleck_native.a"
        $verb = if ($Publish) { 'publish' } else { 'build' }
        $projectRoot = Split-Path -Parent $project
        $publishArguments = @()
        if ($Publish) {
            # Publishing over an existing directory retains obsolete hashed Wasm
            # assets. Build a complete fresh tree before replacing the old one.
            $publishParent = [IO.Path]::GetFullPath((Join-Path $projectRoot "bin/$Configuration/net10.0"))
            $publishRoot = Join-Path $publishParent 'publish'
            $publishStage = Join-Path $publishParent ('.publish-stage-' + [guid]::NewGuid().ToString('N'))
            $publishArguments = @('--output', $publishStage)
        }
        Invoke-Checked $Dotnet (@($verb, $project, '-c', $Configuration, '--nologo',
            '-p:Platform=AnyCPU', '-p:PlatformTarget=AnyCPU',
            "-p:SunfleckWebFont=$fontPath", "-p:SunfleckNativeLibrary=$output/sunfleck_native.a",
            "-p:SunfleckBidiLicense=$output/SheenBidi-LICENSE.txt",
            "-p:WasmCachePath=$env:EM_CACHE") + $toolchainProperties + $publishArguments)
        if ($Publish) {
            if (!(Test-Path -LiteralPath "$publishStage/wwwroot/index.html")) { throw 'Publish did not create a complete site.' }
            $publishBackup = Join-Path $publishParent ('.publish-previous-' + [guid]::NewGuid().ToString('N'))
            # Both moves are confined to this project's generated release directory.
            foreach ($path in @($publishRoot, $publishStage, $publishBackup)) {
                if (![IO.Path]::GetFullPath($path).StartsWith($publishParent + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
                    throw 'Publish destination escaped the generated release directory.'
                }
            }
            $hadPrevious = Test-Path -LiteralPath $publishRoot
            if ($hadPrevious) { Move-Item -LiteralPath $publishRoot -Destination $publishBackup }
            try { Move-Item -LiteralPath $publishStage -Destination $publishRoot }
            catch {
                if ($hadPrevious -and !(Test-Path -LiteralPath $publishRoot)) { Move-Item -LiteralPath $publishBackup -Destination $publishRoot }
                throw
            }
        }
        $site = if ($Publish) { "$projectRoot/bin/$Configuration/net10.0/publish/wwwroot" } else { "$projectRoot/bin/$Configuration/net10.0/wwwroot" }
        Write-Host "Serve $site with an HTTP server."
    }
} finally {
    foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process') }
    Pop-Location
}
