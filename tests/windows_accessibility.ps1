param([Parameter(Mandatory)][string]$Executable)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class FixtureWindow {
    [DllImport("user32", CharSet=CharSet.Unicode)]
    public static extern IntPtr FindWindow(string className,string title);
    [DllImport("user32")]
    public static extern uint GetWindowThreadProcessId(IntPtr hwnd,out uint pid);
    [DllImport("user32")]
    public static extern bool PostMessage(IntPtr hwnd,uint message,IntPtr w,IntPtr l);
}
"@
$process = Start-Process -FilePath $Executable -PassThru -WindowStyle Hidden
try {
    $deadline = [datetime]::UtcNow.AddSeconds(10)
    do {
        $process.Refresh()
        if ($process.HasExited) { throw "Fixture exited with $($process.ExitCode)" }
        $window = [FixtureWindow]::FindWindow("SunfleckPreview","Sunfleck UIA acceptance")
        [uint32]$ownerId = 0
        if ($window -ne [IntPtr]::Zero) {
            [void][FixtureWindow]::GetWindowThreadProcessId($window,[ref]$ownerId)
            if ($ownerId -eq $process.Id) { break }
        }
        Start-Sleep -Milliseconds 50
    } while ([datetime]::UtcNow -lt $deadline)
    if ($ownerId -ne $process.Id) { throw "Fixture HWND missing" }
    $root = [System.Windows.Automation.AutomationElement]::FromHandle($window)
    function Find-Control([string]$id) {
        $condition = New-Object System.Windows.Automation.PropertyCondition(
            [System.Windows.Automation.AutomationElement]::AutomationIdProperty, $id)
        $element = $root.FindFirst([System.Windows.Automation.TreeScope]::Descendants,$condition)
        if (!$element) { throw "No UI Automation element for $id" }
        $element
    }
    foreach ($entry in @{fileDrag='Drag two paths'; objectDrag='Drag object'; dropTarget='Drop here'}.GetEnumerator()) {
        $control = Find-Control $entry.Key
        if ($control.Current.Name -ne $entry.Value -or $control.Current.BoundingRectangle.IsEmpty) {
            throw "Missing drag-and-drop fixture control: $($entry.Key)"
        }
    }
    $button = Find-Control apply
    if ($button.Current.Name -ne 'Apply') { throw "Incorrect button name" }
    $button.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke()
    $toggle = (Find-Control enabled).GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern)
    $toggle.Toggle()
    if ($toggle.Current.ToggleState -ne 'On') { throw "Toggle failed" }
    $edit = (Find-Control editor).GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)
    $edit.SetValue('updated')
    if ($edit.Current.Value -ne 'updated') { throw "SetValue failed" }
    $secret = Find-Control secret
    if (!$secret.Current.IsPassword) { throw "Password role missing" }
    $password = $secret.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)
    $revealed = $null
    try { $revealed = $password.Current.Value } catch {}
    if ($revealed) { throw "Password value exposed" }
    $slider = (Find-Control level).GetCurrentPattern([System.Windows.Automation.RangeValuePattern]::Pattern)
    $slider.SetValue(68)
    if ($slider.Current.Value -ne 68) { throw "Slider value mismatch" }
    $combo = (Find-Control choices).GetCurrentPattern([System.Windows.Automation.ExpandCollapsePattern]::Pattern)
    $combo.Expand()
    if ($combo.Current.ExpandCollapseState -ne 'Expanded') { throw "Expansion failed" }
    $combo.Collapse()
    if (![FixtureWindow]::PostMessage($window,0x10,[IntPtr]::Zero,[IntPtr]::Zero) -or !$process.WaitForExit(5000) -or $process.ExitCode) { throw "Window close failed" }
} finally {
    if (!$process.HasExited) { $process.Kill(); $process.WaitForExit() }
    $process.Dispose()
}

