# Boot-Time Persistence via Scheduled Task

Fan curves set by IGCL are **volatile** — they are lost when the system
reboots. To automatically re-apply your settings at startup, create a
Windows Task Scheduler task.

## Option 1: PowerShell Script (Recommended)

Run **as Administrator**:

```powershell
.\scripts\install-scheduled-task.ps1
```

This creates a task named "IGCL Fan Control" that runs at every user logon
with highest privileges and hidden window.

To remove:

```powershell
.\scripts\uninstall-scheduled-task.ps1
```

## Option 2: Manual Setup

1. Open **Task Scheduler** (taskschd.msc) as Administrator
2. Click **Create Task**
3. **General** tab:
   - Name: `IGCL Fan Control`
   - Check **Run with highest privileges**
   - Configure for: Windows 10
4. **Triggers** tab → **New**:
   - Begin the task: **At log on**
   - OK
5. **Actions** tab → **New**:
   - Action: Start a program
   - Program: `C:\path\to\igcl-fan-control.exe`
   - Arguments: (leave blank, or add `--config C:\path\to\custom.ini`)
   - Start in: `C:\path\to\igcl-fan-control\`
6. **Conditions** tab: Uncheck everything
7. **Settings** tab:
   - Check **Run task as soon as possible after a scheduled start is missed**
   - Uncheck **Stop the task if it runs longer than:**
8. Click **OK**, enter your password

## Option 3: Registry (Run key)

For a simpler (but less robust) approach, add to the registry:

```
HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run
```

String value:
- Name: `IGCL Fan Control`
- Value: `"C:\path\to\igcl-fan-control.exe"`

This runs with user privileges (not Administrator), which is **not sufficient**
for fan control. Combine with a shortcut configured to "Run as administrator"
or use the scheduled task approach instead.

## Verifying

After reboot, check:

```powershell
igcl-fan-control.exe --info
```

The "Current mode" should show "CUSTOM TABLE (fan curve)" with your configured
points, not "DEFAULT (hardware)."
