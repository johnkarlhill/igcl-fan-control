# uninstall-scheduled-task.ps1
# Removes the IGCL Fan Control scheduled task.

$TaskName = "IGCL Fan Control"
$existing = Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue

if ($existing) {
    Unregister-ScheduledTask -TaskName $TaskName -Confirm:$false
    Write-Host "Task '$TaskName' removed."
} else {
    Write-Host "Task '$TaskName' not found."
}
