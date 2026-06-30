# install-scheduled-task.ps1
# Creates a Windows Task Scheduler task to re-apply IGCL fan curves at logon.
# Must be run as Administrator.

$ErrorActionPreference = "Stop"

$TaskName = "IGCL Fan Control"
$ExePath = Join-Path $PSScriptRoot "..\build\Release\igcl-fan-control.exe"
$WorkingDir = Join-Path $PSScriptRoot ".."

if (-not (Test-Path $ExePath)) {
    Write-Error "Executable not found: $ExePath"
    Write-Error "Build the project first: cmake --build build --config Release"
    exit 1
}

# Remove existing task if present
$existing = Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue
if ($existing) {
    Write-Host "Removing existing task '$TaskName'..."
    Unregister-ScheduledTask -TaskName $TaskName -Confirm:$false
}

$Action = New-ScheduledTaskAction -Execute $ExePath -WorkingDirectory $WorkingDir

$Trigger = New-ScheduledTaskTrigger -AtLogOn

$Principal = New-ScheduledTaskPrincipal -UserId $env:USERNAME -RunLevel Highest

$Settings = New-ScheduledTaskSettingsSet `
    -AllowStartIfOnBatteries `
    -DontStopIfGoingOnBatteries `
    -StartWhenAvailable `
    -Hidden

Register-ScheduledTask `
    -TaskName $TaskName `
    -Action $Action `
    -Trigger $Trigger `
    -Principal $Principal `
    -Settings $Settings `
    -Description "Re-applies Intel GPU fan curves from igcl-fan-control.ini at logon."

Write-Host ""
Write-Host "Task '$TaskName' installed successfully."
Write-Host "  Runs at user logon with highest privileges."
Write-Host "  To remove: .\scripts\uninstall-scheduled-task.ps1"
Write-Host ""
Write-Host "To test now:"
Write-Host "  Start-ScheduledTask -TaskName '$TaskName'"
