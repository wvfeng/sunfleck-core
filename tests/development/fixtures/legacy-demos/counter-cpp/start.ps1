param([ValidateSet('Open', 'Build', 'Run', 'Test')][string]$Action = 'Open')
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../../..'))
$vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
if (!(Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio with Desktop development with C++.' }
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$installation) { throw 'The MSVC x64 toolchain was not found.' }
Import-Module (Join-Path $installation 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $installation -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null

# Prefer the native Windows tools paired with this Visual Studio installation.
# A MinGW CMake earlier on PATH can fail encoding MSVC's localized /showIncludes
# prefix when VS Code launches it through its terminal (std::__ios_failure).
$cmakeTools = Join-Path $installation 'Common7/IDE/CommonExtensions/Microsoft/CMake'
$cmake = Join-Path $cmakeTools 'CMake/bin/cmake.exe'
$ctest = Join-Path $cmakeTools 'CMake/bin/ctest.exe'
$ninja = Join-Path $cmakeTools 'Ninja/ninja.exe'
if (!(Test-Path -LiteralPath $cmake)) { $cmake = (Get-Command cmake -ErrorAction Stop).Source }
if (!(Test-Path -LiteralPath $ctest)) { $ctest = (Get-Command ctest -ErrorAction Stop).Source }
$configureArguments = @('--preset', 'debug')
if (Test-Path -LiteralPath $ninja) { $configureArguments += "-DCMAKE_MAKE_PROGRAM=$ninja" }

Push-Location $PSScriptRoot
$previousPipe = $env:SUNFLECK_DEBUG_PIPE
$previousConsoleEncoding = [Console]::OutputEncoding
try {
    # Keep the configure-time probe and Ninja's compiler subprocesses on the
    # same code page, including when PowerShell is started by VS Code/ConPTY.
    [Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
    Write-Host "CMake: $cmake"
    & $cmake @configureArguments
    if ($LASTEXITCODE) { throw 'C++ counter configuration failed.' }
    & $cmake --build --preset debug
    if ($LASTEXITCODE) { throw 'C++ counter build failed.' }
    if ($Action -eq 'Test') {
        & $ctest --preset debug
        if ($LASTEXITCODE) { throw 'C++ counter tests failed.' }
    } elseif ($Action -eq 'Run') {
        $env:SUNFLECK_DEBUG_PIPE = 'sunfleck-counter-cpp'
        & ./bin/Debug/CounterCpp.exe "$PSScriptRoot/main.fleck"
        if ($LASTEXITCODE) { throw "C++ counter exited with code $LASTEXITCODE." }
    } elseif ($Action -eq 'Open') {
        if (!(Test-Path "$root/out/windows-dev/service/Sunfleck.Development.dll")) {
            & pwsh -NoProfile -File "$root/tools/development/windows-dev.ps1" -Action Prepare
            if ($LASTEXITCODE) { throw 'Development tools could not be built.' }
        }
        $code = "$env:LOCALAPPDATA/Programs/Microsoft VS Code/bin/code.cmd"
        $folder = ([Uri]($PSScriptRoot + '/')).AbsoluteUri
        & $code --new-window "--extensionDevelopmentPath=$root/editors/vscode" --folder-uri $folder
        if ($LASTEXITCODE) { throw 'VS Code could not be opened.' }
        & $code --reuse-window --folder-uri $folder
        if ($LASTEXITCODE) { throw 'VS Code workspace could not be selected.' }
    }
} finally {
    [Console]::OutputEncoding = $previousConsoleEncoding
    $env:SUNFLECK_DEBUG_PIPE = $previousPipe
    Pop-Location
}
