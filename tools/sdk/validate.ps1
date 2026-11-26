param([string]$OutputDirectory, [string]$SkiaRoot, [string]$SkiaLibraryDirectory)
$ErrorActionPreference = 'Stop'
if (!$IsWindows) { throw 'This validation entry targets the Windows x64 SDK.' }
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (!$OutputDirectory) { $OutputDirectory = Join-Path $root ('out/sdk-' + (Get-Date -Format 'yyyyMMddTHHmmss')) }
$output = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $output) { throw "Use a new output directory: $output" }
New-Item -ItemType Directory -Path $output | Out-Null
function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE) { throw "$Program exited with $LASTEXITCODE" }
}
$vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$installation) { throw 'MSVC desktop tools are required.' }
Import-Module (Join-Path $installation 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $installation -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$cmakeRoot = Join-Path $installation 'Common7/IDE/CommonExtensions/Microsoft/CMake'
$cmake = Join-Path $cmakeRoot 'CMake/bin/cmake.exe'
$ctest = Join-Path $cmakeRoot 'CMake/bin/ctest.exe'
$ninja = Join-Path $cmakeRoot 'Ninja/ninja.exe'
$build = Join-Path $output 'build'
$prefix = Join-Path $output 'initial-prefix'
$relocated = Join-Path $output 'relocated SDK'
$consumer = Join-Path ([IO.Path]::GetTempPath()) ('Sunfleck-consumer-' + [guid]::NewGuid().ToString('N'))
$result = [ordered]@{ status='running'; startedUtc=[DateTime]::UtcNow.ToString('o'); output=$output; sdk=$relocated; consumer=$consumer; checks=@(); environment=@{ os=[Environment]::OSVersion.VersionString; architecture='x64'; configuration='Release'; processor=$env:PROCESSOR_IDENTIFIER; visualStudio=$installation } }
try {
    Invoke-Checked python @("$PSScriptRoot/package.py", "$output/build-inputs.json", '--snapshot')
    $rendererOptions = @()
    if ($SkiaLibraryDirectory -and !$SkiaRoot) { throw 'SkiaLibraryDirectory requires SkiaRoot.' }
    if ($SkiaRoot) {
        $rendererOptions += '-DUI_ENABLE_SKIA_D3D12=ON', "-DUI_SKIA_ROOT=$([IO.Path]::GetFullPath($SkiaRoot))"
        if ($SkiaLibraryDirectory) { $rendererOptions += "-DUI_SKIA_LIBRARY_DIR=$([IO.Path]::GetFullPath($SkiaLibraryDirectory))" }
        $result.renderer = 'Skia native D3D12 (Auto), DirectWrite text'
    }
    Invoke-Checked $cmake (@('-S',$root,'-B',$build,'-G','Ninja',"-DCMAKE_MAKE_PROGRAM=$ninja",'-DCMAKE_BUILD_TYPE=Release',
        '-DUI_INSTALL_SDK=ON','-DUI_BUILD_CSHARP=ON','-DUI_BUILD_EXAMPLES=OFF','-DUI_BUILD_DEMOS=OFF','-DBUILD_TESTING=OFF') + $rendererOptions)
    Invoke-Checked $cmake @('--build',$build,'--target','sunfleck','sunfleck_native','fleckc','sunfleck_sdk_managed','-j','1')
    Invoke-Checked $cmake @('--install',$build,'--prefix',$prefix)
    # Both absolute paths are verified inside this newly created output before moving.
    foreach ($target in @($prefix,$relocated)) {
        if (![IO.Path]::GetFullPath($target).StartsWith($output + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'SDK move escaped output directory.' }
    }
    Move-Item -LiteralPath $prefix -Destination $relocated
    Copy-Item -LiteralPath (Join-Path $relocated 'share/sunfleck/examples') -Destination $consumer -Recurse
    Invoke-Checked $cmake @('-S',$consumer,'-B',"$consumer/build",'-G','Ninja',"-DCMAKE_MAKE_PROGRAM=$ninja",'-DCMAKE_BUILD_TYPE=Release',"-DCMAKE_PREFIX_PATH=$relocated")
    Invoke-Checked $cmake @('--build',"$consumer/build",'-j','1')
    $dependencies = & $ninja -C "$consumer/build" -t deps CMakeFiles/consumer_cpp.dir/main.cpp.obj
    if ($LASTEXITCODE -or !($dependencies -match 'sunfleck[/\\]fleck_page\.hpp')) {
        throw 'Installed C++ headers are missing from Ninja dependencies.'
    }
    $dependencies | Set-Content "$output/header-dependencies.log" -Encoding utf8
    $result.checks += 'localized-msvc-header-dependencies'
    Invoke-Checked $ctest @('--test-dir',"$consumer/build",'--output-on-failure','--output-junit',"$output/consumers.xml")
    $result.checks += 'relocated-installed-c-and-cpp-with-generated-bindings'
    Invoke-Checked dotnet @('build',"$consumer/Consumer.csproj",'-c','Release',"-p:SunfleckSdkRoot=$relocated",'-o',"$consumer/managed",'--nologo')
    Invoke-Checked dotnet @("$consumer/managed/Consumer.dll")
    $result.checks += 'installed-csharp-managed-and-native'
    $imports = @{}
    foreach ($file in Get-ChildItem -LiteralPath "$relocated/bin" -File) {
        if ($file.Extension -in '.dll','.exe') {
            $dump = & dumpbin /dependents $file.FullName
            if ($LASTEXITCODE) { throw 'PE import inspection failed.' }
            $imports[$file.Name] = @($dump | Where-Object { $_ -match '^\s+[\w.-]+\.dll\s*$' } | ForEach-Object { $_.Trim() })
        }
    }
    $imports | ConvertTo-Json -Depth 5 | Set-Content "$output/pe-dependencies.json" -Encoding utf8
    if ($SkiaRoot) {
        if (@($imports.Values | ForEach-Object { $_ } | Where-Object { $_ -match '^lib(EGL|GLESv2)\.dll$' }).Count) {
            throw 'Native D3D12 SDK unexpectedly imports ANGLE.'
        }
        $result.skiaSource = Get-Content "$build/skia-source-candidate.json" -Raw | ConvertFrom-Json -AsHashtable
        $result.checks += 'skia-source-candidate-and-no-angle-imports'
    }
    $result.checks += 'pe-runtime-dependencies-recorded'
    $result.files = @{}
    foreach ($file in Get-ChildItem -LiteralPath $relocated -Recurse -File) {
        $name = [IO.Path]::GetRelativePath($relocated, $file.FullName).Replace('\','/')
        $result.files[$name] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    Invoke-Checked python @("$PSScriptRoot/package.py", "$output/build-inputs-after.json", '--snapshot')
    if ((Get-FileHash "$output/build-inputs.json").Hash -ne (Get-FileHash "$output/build-inputs-after.json").Hash) {
        throw 'SDK source changed during build/validation; rerun with stable inputs.'
    }
    $result.buildInputs = Get-Content "$output/build-inputs.json" -Raw | ConvertFrom-Json -AsHashtable
    $result.status = 'passed'
} catch { $result.status='failed'; $result.error=$_.Exception.Message; throw }
finally {
    $result.finishedUtc=[DateTime]::UtcNow.ToString('o')
    $result | ConvertTo-Json -Depth 8 | Set-Content "$output/result.json" -Encoding utf8
    Write-Host "SDK evidence: $output"
}
