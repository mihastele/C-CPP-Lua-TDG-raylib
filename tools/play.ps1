# Drives the real towerfall window: picks towers from the sidebar, builds them on the
# map, and captures screenshots. This is the end-to-end proof that the game is playable
# by a human, not just that the simulation is correct.
#
# Usage:  powershell -File tools/play.ps1 -Exe build/towerfall.exe -Out docs
param(
    [string]$Exe = "build/towerfall.exe",
    [string]$Out = "docs"
)

Add-Type -AssemblyName System.Windows.Forms, System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Win {
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, IntPtr e);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr h, int x, int y, int w, int ht, bool repaint);
    [DllImport("user32.dll")] public static extern bool GetCursorPos(out POINT p);
    public struct RECT { public int Left, Top, Right, Bottom; }
    public struct POINT { public int X, Y; }
    public const uint LEFTDOWN = 0x0002, LEFTUP = 0x0004;
    public static void Click(int x, int y) {
        SetCursorPos(x, y);
        System.Threading.Thread.Sleep(120);
        mouse_event(LEFTDOWN, 0, 0, 0, IntPtr.Zero);
        System.Threading.Thread.Sleep(60);
        mouse_event(LEFTUP, 0, 0, 0, IntPtr.Zero);
        System.Threading.Thread.Sleep(120);
    }
}
"@

function Shot($path) {
    $p = Get-Process towerfall -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $p) { Write-Host "no process"; return }
    $r = New-Object Win+RECT
    [Win]::GetWindowRect($p.MainWindowHandle, [ref]$r) | Out-Null
    $w = $r.Right - $r.Left; $h = $r.Bottom - $r.Top
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size)
    $bmp.Save($path)
    Write-Host "saved $path"
}

# Start the game.
Get-Process towerfall -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 500
$p = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Seconds 3

$p = Get-Process towerfall -ErrorAction SilentlyContinue | Select-Object -First 1
[Win]::ShowWindow($p.MainWindowHandle, 9) | Out-Null
[Win]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
# Pin the window to the top-left, so the click coordinates are predictable.
[Win]::MoveWindow($p.MainWindowHandle, 0, 0, 1286, 749, $true) | Out-Null
Start-Sleep -Seconds 1

$r = New-Object Win+RECT
[Win]::GetWindowRect($p.MainWindowHandle, [ref]$r) | Out-Null
$wx = $r.Left; $wy = $r.Top
Write-Host "window at $wx,$wy size $($r.Right-$r.Left)x$($r.Bottom-$r.Top)"

# The window is 1280x720 but the frame adds a title bar, so the client area starts
# about 31 pixels below the top of the window frame.
$clientY = $wy + 31   # the title bar is about 31px tall

# ---- the sidebar's tower buttons (window coordinates)
$canvasW = 1280 - 300
$shopX = $wx + $canvasW + 90
function ShopY($i) { return $clientY + 36 + $i * 46 + 20 }

# ---- the map geometry, matching the renderer's calculation
$ts = [Math]::Min([int]($canvasW / 24), [int]((720 - 40) / 14))
$mapW = $ts * 24; $mapH = $ts * 14
$mapX = [int](($canvasW - $mapW) / 2)
$mapY = 40 + [int]((720 - 40 - $mapH) / 2)
Write-Host "tile=$ts map at $mapX,$mapY"

# NOTE: PowerShell unrolls an array returned from a function, so the coordinates
# are returned through script-scope variables rather than a return value.
function MapPt($cx, $cy) {
    $script:ptX = $wx + $mapX + $cx * $ts + [int]($ts/2)
    $script:ptY = $clientY + $mapY + $cy * $ts + [int]($ts/2)
}

# ---- build a line of archers along the route
Shot("$Out/shot-play-0.png")

$arrowIndex = 0            # Archer Post
[Win]::Click($shopX, (ShopY $arrowIndex))

$cells = @(@(2,10), @(3,10), @(4,10), @(5,10), @(6,10))
foreach ($c in $cells) {
    MapPt $c[0] $c[1]
    [Win]::Click($script:ptX, $script:ptY)
    Write-Host "  click cell $($c[0]),$($c[1]) at $script:ptX,$script:ptY"
}
Start-Sleep -Seconds 1
Shot("$Out/shot-play-1.png")
Write-Host "built $($cells.Count) towers"

# ---- call the wave early and watch it play
[Win]::SetCursorPos($wx + 400, $clientY + 20)
# SPACE via SendKeys
Add-Type -AssemblyName System.Windows.Forms
[System.Windows.Forms.SendKeys]::SendWait(" ")
Start-Sleep -Seconds 6
Shot("$Out/shot-play-2.png")
Write-Host "wave running"

Start-Sleep -Seconds 8
Shot("$Out/shot-play-3.png")

Get-Process towerfall -ErrorAction SilentlyContinue | Stop-Process -Force
Write-Host "done"
