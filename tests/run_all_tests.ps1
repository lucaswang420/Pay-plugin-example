param(
    [string]$ExePath
)

if (-not $ExePath -or $ExePath.Trim().Length -eq 0) {
    Write-Host "Missing PayBackendTests.exe path."
    exit 1
}

if (-not (Test-Path $ExePath)) {
    Write-Host "PayBackendTests.exe not found at: $ExePath"
    exit 1
}

# Run from the exe directory so ./config.json and ./.env resolve.
$exeDir = Split-Path $ExePath -Parent
Set-Location $exeDir

# Single-process full run. With no -r flag, Drogon's test runner executes every
# DROGON_TEST case in one process and prints:
#   - each FAILED assertion block (always), and
#   - a built-in summary line, e.g. "test cases: N | X passed | Y failed"
# (or "All tests passed (M assertions in N tests cases)." when green).
# This mirrors the Linux/macOS add_test() branch and keeps output minimal:
# failures plus a pass/fail/total tally, with no per-test process spin-up.
#
# Stream stdout+stderr to the console so failures and the summary are visible
# under `ctest --output-on-failure`. The process exit code (0 = all passed,
# non-zero = some assertion failed) propagates to CTest unchanged.
& $ExePath 2>&1 | ForEach-Object { Write-Host $_ }
exit $LASTEXITCODE
