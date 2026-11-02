# Copyright (c) 2026 John Pursey
#
# Use of this source code is governed by an MIT-style License that can be found
# in the LICENSE file or at https://opensource.org/licenses/MIT.

# Runs a windowed program, drives it with a short script of steps, and saves
# screenshots of its window, so a session can check what it draws.
#
# Usage:
#   tools\run-program.ps1 -Exe <path> [-Steps <steps>] [-OutDir <dir>]
#                         [-WorkingDirectory <dir>] [-CloseSecs <seconds>]
#
# Steps are run in order, separated by commas:
#   wait:<seconds>   Waits, so the program can load or simulate.
#   key:<name>       Presses and releases a key: a letter or digit, or one of
#                    space, enter, escape, tab, up, down, left, right, f1-f12.
#   hold:<name>:<s>  Holds a key down for <s> seconds.
#   shot:<name>      Saves a screenshot of the window to <OutDir>\<name>.png.
#
# The default steps capture the window once it has had time to draw. After the
# last step, the program's window is closed as its close button would, so the
# program shuts down normally. The run fails if the program exits during the
# steps, exits with an error code, or is still running <CloseSecs> after being
# closed. In that last case, the stack of each of its threads is saved to
# <OutDir>\hang-stacks.txt (see dump-stacks.ps1) before the program is ended.
# Its stdout and stderr (the log) are saved to <OutDir>, which defaults to
# out\run\<program name> in the current directory.
#
# Keys are sent to the program's window, and screenshots are taken from it, so
# it doesn't need focus and nothing is typed into other windows. It may be
# behind other windows, but not minimized. There is no mouse step: ImGui reads
# the mouse from the real cursor, which a script can't move without taking over
# the user's mouse.

param(
  [Parameter(Mandatory = $true)]
  [string]$Exe,
  [string]$Steps = "wait:3,shot:window",
  [string]$OutDir = "",
  [string]$WorkingDirectory = "",
  [double]$CloseSecs = 15
)

$ErrorActionPreference = "Stop"
if (-not (Test-Path $Exe)) {
  Write-Error "$Exe is not built."
}
$Exe = (Resolve-Path $Exe).Path
$name = [System.IO.Path]::GetFileNameWithoutExtension($Exe)
if ($OutDir -eq "") {
  $OutDir = Join-Path "out\run" $name
}
if ($WorkingDirectory -eq "") {
  $WorkingDirectory = (Get-Location).Path
}

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class RunProgramWindow {
  [StructLayout(LayoutKind.Sequential)]
  public struct RECT { public int Left, Top, Right, Bottom; }
  [DllImport("user32.dll")]
  public static extern bool GetClientRect(IntPtr window, out RECT rect);
  [DllImport("user32.dll")]
  public static extern bool IsIconic(IntPtr window);
  [DllImport("user32.dll")]
  public static extern bool PrintWindow(IntPtr window, IntPtr hdc, uint flags);
  [DllImport("user32.dll")]
  public static extern bool PostMessage(IntPtr window, uint message,
                                        IntPtr wParam, IntPtr lParam);
  [DllImport("user32.dll")]
  public static extern uint MapVirtualKey(uint code, uint mapType);
  [DllImport("user32.dll")]
  public static extern bool SetProcessDPIAware();
}
"@

# Without this, window sizes are scaled on high DPI displays, and screenshots
# are cropped.
[RunProgramWindow]::SetProcessDPIAware() | Out-Null

# Named keys, each with its virtual key code and whether it is an extended key.
# An extended key's messages must say so, or the program reads it as the number
# pad key that shares its scan code.
$namedKeys = @{
  space = @(0x20, $false); enter = @(0x0D, $false)
  escape = @(0x1B, $false); tab = @(0x09, $false)
  left = @(0x25, $true); up = @(0x26, $true)
  right = @(0x27, $true); down = @(0x28, $true)
}
for ($i = 1; $i -le 12; $i++) {
  $namedKeys["f$i"] = @((0x6F + $i), $false)
}

# Returns a key's virtual key code, and whether it is an extended key.
function Get-Key([string]$keyName) {
  $keyName = $keyName.ToLower()
  if ($namedKeys.ContainsKey($keyName)) {
    return $namedKeys[$keyName]
  }
  if ($keyName -match '^[a-z0-9]$') {
    return @([int][char]$keyName.ToUpper(), $false)
  }
  throw "Unknown key '$keyName'."
}

function Get-ProgramWindow {
  $process.Refresh()
  if ($process.HasExited) {
    throw "$name exited during the steps."
  }
  return $process.MainWindowHandle
}

function Send-Key([string]$keyName, [double]$holdSecs) {
  $window = Get-ProgramWindow
  $vk, $extended = Get-Key $keyName

  # The key message carries the key's scan code, and whether it is extended,
  # which SDL reads to tell which key it is.
  $scan = [RunProgramWindow]::MapVirtualKey($vk, 0)  # MAPVK_VK_TO_VSC
  [int64]$lParam = 1 -bor ($scan -shl 16)
  if ($extended) {
    $lParam = $lParam -bor (1 -shl 24)
  }
  [RunProgramWindow]::PostMessage($window, 0x0100, [IntPtr]$vk,
                                  [IntPtr]$lParam) | Out-Null  # WM_KEYDOWN
  Start-Sleep -Milliseconds ([int]($holdSecs * 1000))

  # A key up also sets the previous state and transition bits.
  $lParam = $lParam -bor ([int64]3 -shl 30)
  [RunProgramWindow]::PostMessage($window, 0x0101, [IntPtr]$vk,
                                  [IntPtr]$lParam) | Out-Null  # WM_KEYUP
}

function Save-Screenshot([string]$shotName) {
  $window = Get-ProgramWindow
  if ([RunProgramWindow]::IsIconic($window)) {
    throw "The $name window is minimized, so it can't be captured."
  }
  $rect = New-Object RunProgramWindow+RECT
  [RunProgramWindow]::GetClientRect($window, [ref]$rect) | Out-Null
  if ($rect.Right -le 0 -or $rect.Bottom -le 0) {
    throw "The $name window has no size yet, so it can't be captured."
  }
  $bitmap = New-Object System.Drawing.Bitmap $rect.Right, $rect.Bottom
  $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
  $hdc = $graphics.GetHdc()

  # PW_CLIENTONLY | PW_RENDERFULLCONTENT, which captures what the program
  # presented with the GPU, even while the window is behind others.
  $captured = [RunProgramWindow]::PrintWindow($window, $hdc, 3)
  $graphics.ReleaseHdc($hdc)
  if (-not $captured) {
    throw "Could not capture the $name window."
  }
  $path = Join-Path $OutDir "$shotName.png"
  $bitmap.Save($path)
  $graphics.Dispose()
  $bitmap.Dispose()
  "Saved $path"
}

# Closes the program's window, and waits for the program to exit. Adds to
# $failures if it doesn't exit, or exits with an error code.
function Close-Program {
  $process.Refresh()
  if (-not $process.HasExited -and $process.MainWindowHandle -eq 0) {
    # With no window to close, it can only be ended.
    Stop-Process -Id $process.Id -ErrorAction SilentlyContinue
    $process.WaitForExit(10000) | Out-Null
    return
  }
  if (-not $process.HasExited) {
    $timer = [Diagnostics.Stopwatch]::StartNew()
    $process.CloseMainWindow() | Out-Null
    if (-not $process.WaitForExit([int]($CloseSecs * 1000))) {
      $stacks = Join-Path $OutDir "hang-stacks.txt"
      try {
        & (Join-Path $PSScriptRoot "dump-stacks.ps1") -ProcessId $process.Id |
          Out-File -Encoding utf8 $stacks
        "Saved thread stacks to $stacks"
      } catch {
        "Could not save thread stacks: $_"
      }
      Stop-Process -Id $process.Id -ErrorAction SilentlyContinue
      $process.WaitForExit(10000) | Out-Null
      $failures.Add(
        "$name was still running $CloseSecs seconds after being closed.")
      return
    }
    "Closed $name in $($timer.ElapsedMilliseconds) ms."
  }
  if ($process.ExitCode -ne 0) {
    $failures.Add("$name exited with code $($process.ExitCode).")
  }
}

# A full path, as .NET (which saves the screenshots) doesn't share PowerShell's
# current location.
$OutDir = (New-Item -ItemType Directory -Force $OutDir).FullName
$process = Start-Process -FilePath $Exe -WorkingDirectory $WorkingDirectory `
  -PassThru `
  -RedirectStandardOutput (Join-Path $OutDir "stdout.txt") `
  -RedirectStandardError (Join-Path $OutDir "stderr.txt")

# Holding the handle keeps the exit code readable after the program exits.
$null = $process.Handle
$failures = New-Object System.Collections.Generic.List[string]
try {
  for ($i = 0; $i -lt 80 -and $process.MainWindowHandle -eq 0; $i++) {
    if ($process.HasExited) {
      throw "$name exited before opening a window."
    }
    Start-Sleep -Milliseconds 250
    $process.Refresh()
  }
  if ($process.MainWindowHandle -eq 0) {
    throw "$name did not open a window within 20 seconds."
  }
  foreach ($step in $Steps.Split(",")) {
    $parts = $step.Trim().Split(":")
    $argCount = @{ wait = 1; key = 1; hold = 2; shot = 1 }[$parts[0]]
    if ($argCount -ne $null -and $parts.Count -ne $argCount + 1) {
      throw "Step '$step' needs $argCount value(s) after its name."
    }
    switch ($parts[0]) {
      "wait" { Start-Sleep -Milliseconds ([int]([double]$parts[1] * 1000)) }
      "key" { Send-Key $parts[1] 0.1 }
      "hold" { Send-Key $parts[1] ([double]$parts[2]) }
      "shot" { Save-Screenshot $parts[1] }
      default { throw "Unknown step '$step'." }
    }
  }
} catch {
  $failures.Add("$_")
} finally {
  Close-Program
  "Log: $(Join-Path $OutDir 'stderr.txt')"
}
if ($failures.Count -gt 0) {
  throw ($failures -join " ")
}
