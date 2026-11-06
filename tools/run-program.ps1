# Copyright (c) 2026 John Pursey
#
# Use of this source code is governed by an MIT-style License that can be found
# in the LICENSE file or at https://opensource.org/licenses/MIT.

# Runs a program built on Game Bits with a pilot script, so a session can check
# what it draws and how it responds without the user.
#
# Usage:
#   tools\run-program.ps1 -Exe <path> [-Script <file> | -Steps <steps>]
#                         [-Arguments <arguments>] [-OutDir <dir>]
#                         [-WorkingDirectory <dir>] [-TimeoutSecs <seconds>]
#
# The program must take the pilot flags (--pilot and --pilot-output, from
# gb_pilot_script), and runs the script by itself: src/gb/pilot/script/
# pilot_script.h lists the steps. -Steps is a script written inline, with ";"
# between steps (so ";" can't appear in a step's text), which is saved to
# <OutDir>\steps.txt. With neither, the steps save one frame once the program
# has had time to draw. -Arguments are the program's other arguments, as they
# would be typed after it on a command line.
#
# The program exits once the script ends. The run fails if it exits with an
# error code, which it does if a step failed or an error was logged, or is still
# running <TimeoutSecs> after it started. In that last case, the stack of each
# of its threads is saved to <OutDir>\hang-stacks.txt (see dump-stacks.ps1)
# before the program is ended.
#
# Saved frames go to <OutDir>, which defaults to out\run\<program name> in the
# current directory, as <name>.png. The program's stdout and stderr (the log)
# are saved there too. At the end, the frames this run saved, the log's errors,
# and the run's summary are printed.

param(
  [Parameter(Mandatory = $true)]
  [string]$Exe,
  [string]$Script = "",
  [string]$Steps = "hide window; wait 30; save frame",
  [string]$Arguments = "",
  [string]$OutDir = "",
  [string]$WorkingDirectory = "",
  [double]$TimeoutSecs = 60
)

$ErrorActionPreference = "Stop"
if (-not (Test-Path $Exe)) {
  Write-Error "$Exe is not built."
}
if ($Script -ne "" -and $PSBoundParameters.ContainsKey("Steps")) {
  Write-Error "Give -Script or -Steps, not both."
}
$Exe = (Resolve-Path $Exe).Path
$name = [System.IO.Path]::GetFileNameWithoutExtension($Exe)
if ($OutDir -eq "") {
  $OutDir = Join-Path "out\run" $name
}
if ($WorkingDirectory -eq "") {
  $WorkingDirectory = (Get-Location).Path
}

# Full paths, as the program runs in its own working directory.
$OutDir = (New-Item -ItemType Directory -Force $OutDir).FullName
if ($Script -ne "") {
  $Script = (Resolve-Path $Script).Path
} else {
  # UTF-8 without a byte order mark, which the script reader doesn't take.
  $Script = Join-Path $OutDir "steps.txt"
  [System.IO.File]::WriteAllText($Script, $Steps.Replace(";", "`n"))
}

$log = Join-Path $OutDir "stderr.txt"
$started = Get-Date
$process = Start-Process -FilePath $Exe -WorkingDirectory $WorkingDirectory `
  -ArgumentList "$Arguments --pilot `"$Script`" --pilot-output `"$OutDir`"" `
  -PassThru `
  -RedirectStandardOutput (Join-Path $OutDir "stdout.txt") `
  -RedirectStandardError $log

# Holding the handle keeps the exit code readable after the program exits.
$null = $process.Handle
$failure = ""
if ($process.WaitForExit([int]($TimeoutSecs * 1000))) {
  if ($process.ExitCode -ne 0) {
    $failure = "$name exited with code $($process.ExitCode)."
  }
} else {
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
  $failure = "$name was still running $TimeoutSecs seconds after it started."
}

Get-ChildItem $OutDir -Filter *.png |
  Where-Object { $_.LastWriteTime -ge $started } | Sort-Object LastWriteTime |
  ForEach-Object { "Saved $($_.FullName)" }
"Log: $log"

# Errors and fatal errors, as Abseil logs them, and the pilot's summary.
Select-String -Path $log -Pattern '^[EF]\d{4} |Pilot run ' |
  ForEach-Object Line
if ($failure -ne "") {
  throw $failure
}
