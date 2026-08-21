[CmdletBinding()]
param(
    [string]$BuildDirectory = "out/build/vs2022-x64-release",
    [ValidateRange(1, 600)]
    [int]$DurationSeconds = 2,
    [ValidateRange(1, 100)]
    [int]$RestartCount = 1,
    [switch]$SkipWindowScenarios
)

$ErrorActionPreference = "Stop"

$buildRoot = (Resolve-Path -LiteralPath $BuildDirectory).Path
$exampleDirectory = Join-Path $buildRoot "examples/Release"
$probe = Join-Path $exampleDirectory "media_capture_device_probe.exe"
$windowFixture = Join-Path $exampleDirectory "media_capture_window_fixture.exe"

if (-not (Test-Path -LiteralPath $probe -PathType Leaf)) {
    throw "Device probe was not found: $probe"
}

function Invoke-DeviceProbe {
    param([string[]]$ProbeArguments)

    & $probe @ProbeArguments
    if ($LASTEXITCODE -ne 0) {
        throw "Device probe failed ($LASTEXITCODE): $($ProbeArguments -join ' ')"
    }
}

function Invoke-WindowScenario {
    param([ValidateSet("minimize", "hide")][string]$Mode)

    if (-not (Test-Path -LiteralPath $windowFixture -PathType Leaf)) {
        throw "Window fixture was not found: $windowFixture"
    }

    $suffix = [Guid]::NewGuid().ToString("N")
    $standardOutput = Join-Path ([IO.Path]::GetTempPath()) "media-capture-$suffix.out"
    $standardError = Join-Path ([IO.Path]::GetTempPath()) "media-capture-$suffix.err"
    $fixtureProcess = $null
    try {
        $fixtureProcess = Start-Process -FilePath $windowFixture `
            -ArgumentList @("12", "3", "7", $Mode) `
            -RedirectStandardOutput $standardOutput `
            -RedirectStandardError $standardError `
            -PassThru

        $deadline = (Get-Date).AddSeconds(3)
        do {
            Start-Sleep -Milliseconds 100
            $sourceId = Get-Content -LiteralPath $standardOutput -ErrorAction SilentlyContinue |
                Select-Object -First 1
        } while (-not $sourceId -and (Get-Date) -lt $deadline)

        if (-not $sourceId) {
            $fixtureError = Get-Content -LiteralPath $standardError -ErrorAction SilentlyContinue
            throw "Window fixture did not publish a source ID: $fixtureError"
        }
        Invoke-DeviceProbe -ProbeArguments @("window", $sourceId, "10")
    }
    finally {
        if ($fixtureProcess -and -not $fixtureProcess.HasExited) {
            Stop-Process -Id $fixtureProcess.Id -Force
        }
        Remove-Item -LiteralPath $standardOutput, $standardError -Force -ErrorAction SilentlyContinue
    }
}

for ($cycle = 1; $cycle -le $RestartCount; ++$cycle) {
    Write-Host "capture restart cycle $cycle/$RestartCount"
    Invoke-DeviceProbe -ProbeArguments @("audio", "$DurationSeconds")
    Invoke-DeviceProbe -ProbeArguments @("camera", "$DurationSeconds")
    Invoke-DeviceProbe -ProbeArguments @("screen", "$DurationSeconds")
}

$cameraModes = @(
    @{ Width = "640"; Height = "480"; Fps = "30" },
    @{ Width = "1280"; Height = "720"; Fps = "15" },
    @{ Width = "1280"; Height = "720"; Fps = "30" },
    @{ Width = "1920"; Height = "1080"; Fps = "30" }
)
foreach ($mode in $cameraModes) {
    Invoke-DeviceProbe -ProbeArguments @(
        "camera", "default", "$DurationSeconds", $mode.Width, $mode.Height, $mode.Fps
    )
}

if (-not $SkipWindowScenarios) {
    Invoke-WindowScenario "minimize"
    Invoke-WindowScenario "hide"
}

Write-Host "media-capture hardware acceptance passed"
