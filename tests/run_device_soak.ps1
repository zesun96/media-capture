[CmdletBinding()]
param(
    [string]$BuildDirectory = "out/build/vs2022-x64-release",
    [ValidateRange(1, 3600)]
    [int]$DurationSeconds = 600,
    [string]$OutputDirectory = "out/logs/release-acceptance/soak"
)

$ErrorActionPreference = "Stop"

# Start-Process builds a case-insensitive environment block on Windows and rejects inherited
# environments that contain both PATH and Path.
$processPath = $env:Path
[Environment]::SetEnvironmentVariable("PATH", $null, [EnvironmentVariableTarget]::Process)
[Environment]::SetEnvironmentVariable("Path", $processPath, [EnvironmentVariableTarget]::Process)

$buildRoot = (Resolve-Path -LiteralPath $BuildDirectory).Path
$probe = Join-Path $buildRoot "examples/Release/media_capture_device_probe.exe"
if (-not (Test-Path -LiteralPath $probe -PathType Leaf)) {
    throw "Device probe was not found: $probe"
}

$outputRoot = [IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputDirectory))
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null

$specifications = @(
    @{ Name = "audio"; Arguments = @("audio", "$DurationSeconds") },
    @{ Name = "camera"; Arguments = @("camera", "default", "$DurationSeconds", "1280", "720", "30") },
    @{ Name = "screen"; Arguments = @("screen", "$DurationSeconds") }
)

$processes = @()
try {
    foreach ($specification in $specifications) {
        $standardOutput = Join-Path $outputRoot "$($specification.Name).log"
        $standardError = Join-Path $outputRoot "$($specification.Name).err.log"
        $process = Start-Process -FilePath $probe `
            -ArgumentList $specification.Arguments `
            -RedirectStandardOutput $standardOutput `
            -RedirectStandardError $standardError `
            -WindowStyle Hidden `
            -PassThru
        $processes += @{
            Name = $specification.Name
            Process = $process
            StandardOutput = $standardOutput
            StandardError = $standardError
        }
    }

    $minimumEnd = (Get-Date).AddSeconds([Math]::Max(0, $DurationSeconds - 1))
    while ((Get-Date) -lt $minimumEnd) {
        foreach ($entry in $processes) {
            if ($entry.Process.HasExited) {
                $errorOutput = Get-Content -LiteralPath $entry.StandardError -ErrorAction SilentlyContinue
                throw "$($entry.Name) soak probe exited before $DurationSeconds seconds: $errorOutput"
            }
        }
        Start-Sleep -Seconds 1
    }

    $deadline = (Get-Date).AddSeconds(31)
    foreach ($entry in $processes) {
        $remaining = [Math]::Max(0, [int][Math]::Ceiling(($deadline - (Get-Date)).TotalMilliseconds))
        if (-not $entry.Process.WaitForExit($remaining)) {
            throw "$($entry.Name) soak probe did not stop within the timeout"
        }
        # Complete redirected-stream handling before reading ExitCode. The timed overload alone can
        # leave the PowerShell Process wrapper without a populated exit code on Windows.
        $entry.Process.WaitForExit()
    }

    foreach ($entry in $processes) {
        $exitCode = $entry.Process.ExitCode
        # Windows PowerShell 5.1 can leave ExitCode unset for Start-Process instances with both
        # output streams redirected. A reported nonzero code is still authoritative; the result
        # record below is the portable success criterion when the property is unavailable.
        if ($null -ne $exitCode -and $exitCode -ne 0) {
            $errorOutput = Get-Content -LiteralPath $entry.StandardError -ErrorAction SilentlyContinue
            throw "$($entry.Name) soak probe failed (exit code: $exitCode): $errorOutput"
        }
        $metrics = Get-Content -LiteralPath $entry.StandardOutput -ErrorAction Stop | Select-Object -Last 1
        if ($metrics -notmatch "result=pass" -or $metrics -notmatch "invalid_frames=0" -or
            $metrics -notmatch "callbacks_after_stop=0") {
            throw "$($entry.Name) soak probe did not report a clean stop: $metrics"
        }
        Write-Host "$($entry.Name) soak passed: $metrics"
    }
}
finally {
    foreach ($entry in $processes) {
        if (-not $entry.Process.HasExited) {
            Stop-Process -Id $entry.Process.Id -Force
        }
    }
}

Write-Host "media-capture $DurationSeconds-second parallel hardware soak passed"
