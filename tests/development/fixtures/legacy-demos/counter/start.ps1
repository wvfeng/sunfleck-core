param([ValidateSet('Open', 'Run', 'Watch')][string]$Action = 'Open')
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../../..'))
if (!(Test-Path "$root/out/windows-dev/service/Sunfleck.Development.dll") -or
    !(Test-Path "$root/out/windows-dev/native/sunfleck_native.dll")) {
    & pwsh -NoProfile -File "$root/tools/development/windows-dev.ps1" -Action Prepare
    if ($LASTEXITCODE) { throw 'Development tools could not be built.' }
}
$previousWatch = $env:SunfleckWindowsWatch
$previousPipe = $env:SUNFLECK_DEBUG_PIPE
Push-Location $PSScriptRoot
try {
    $env:SunfleckWindowsWatch = if ($Action -eq 'Watch') { 'true' } else { 'false' }
    if ($Action -in @('Run', 'Watch')) { $env:SUNFLECK_DEBUG_PIPE = 'sunfleck-counter' }
    if ($Action -eq 'Watch') { & dotnet watch run --no-launch-profile }
    elseif ($Action -eq 'Run') { & dotnet run --no-launch-profile }
    else {
        & dotnet build --nologo
        if ($LASTEXITCODE) { throw 'Counter build failed.' }
        $code = "$env:LOCALAPPDATA/Programs/Microsoft VS Code/bin/code.cmd"
        $folder = ([Uri]($PSScriptRoot + '/')).AbsoluteUri
        & $code --new-window "--extensionDevelopmentPath=$root/editors/vscode" --folder-uri $folder
        if ($LASTEXITCODE) { throw 'VS Code could not be opened.' }
        # VS Code can reuse an existing host for this extension and retain its
        # old workspace. A separate ordinary open request switches that host.
        & $code --reuse-window --folder-uri $folder
    }
    if ($LASTEXITCODE) { throw "dotnet failed with exit code $LASTEXITCODE." }
} finally {
    $env:SunfleckWindowsWatch = $previousWatch
    $env:SUNFLECK_DEBUG_PIPE = $previousPipe
    Pop-Location
}
