# =============================================================================
# Pay Plugin Performance Test (PowerShell)
# =============================================================================

$BaseUrl = "http://localhost:5566"
$Results = @()

Write-Host "============================================" -ForegroundColor Cyan
Write-Host "Pay Plugin Performance Test" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

# Test 1: Metrics endpoint
Write-Host "[1/4] Testing /metrics endpoint..." -ForegroundColor Yellow
$times = @()
for ($i = 0; $i -lt 100; $i++) {
    $start = Get-Date
    try {
        $response = Invoke-WebRequest -Uri "$BaseUrl/metrics" -UseBasicParsing -TimeoutSec 5
        $end = Get-Date
        $duration = ($end - $start).TotalMilliseconds
        $times += $duration
    } catch {
        $times += 9999  # Timeout or error
    }
}

$avg = ($times | Measure-Object -Average).Average
$p50 = ($times | Sort-Object)[50]
$p95 = ($times | Sort-Object)[95]
$p99 = ($times | Sort-Object)[99]

Write-Host "  Average: $($avg.ToString('F2')) ms" -ForegroundColor Green
Write-Host "  P50: $($p50.ToString('F2')) ms" -ForegroundColor Green
Write-Host "  P95: $($p95.ToString('F2')) ms" -ForegroundColor Green
Write-Host "  P99: $($p99.ToString('F2')) ms" -ForegroundColor Green
$Results += [PSCustomObject]@{Endpoint="/metrics"; Avg=$avg; P50=$p50; P95=$p95; P99=$p99}
Write-Host ""

# Test 2: Query order endpoint
Write-Host "[2/4] Testing /pay/query endpoint..." -ForegroundColor Yellow
$times = @()
for ($i = 0; $i -lt 100; $i++) {
    $start = Get-Date
    try {
        $response = Invoke-WebRequest -Uri "$BaseUrl/pay/query?order_no=test_$i" -UseBasicParsing -TimeoutSec 5
        $end = Get-Date
        $duration = ($end - $start).TotalMilliseconds
        $times += $duration
    } catch {
        $times += 9999
    }
}

$avg = ($times | Measure-Object -Average).Average
$p50 = ($times | Sort-Object)[50]
$p95 = ($times | Sort-Object)[95]
$p99 = ($times | Sort-Object)[99]

Write-Host "  Average: $($avg.ToString('F2')) ms" -ForegroundColor Green
Write-Host "  P50: $($p50.ToString('F2')) ms" -ForegroundColor Green
Write-Host "  P95: $($p95.ToString('F2')) ms" -ForegroundColor Green
Write-Host "  P99: $($p99.ToString('F2')) ms" -ForegroundColor Green
$Results += [PSCustomObject]@{Endpoint="/pay/query"; Avg=$avg; P50=$p50; P95=$p95; P99=$p99}
Write-Host ""

# Test 3: Query refund endpoint
Write-Host "[3/4] Testing /pay/refund/query endpoint..." -ForegroundColor Yellow
$times = @()
for ($i = 0; $i -lt 100; $i++) {
    $start = Get-Date
    try {
        $response = Invoke-WebRequest -Uri "$BaseUrl/pay/refund/query?refund_no=test_$i" -UseBasicParsing -TimeoutSec 5
        $end = Get-Date
        $duration = ($end - $start).TotalMilliseconds
        $times += $duration
    } catch {
        $times += 9999
    }
}

$avg = ($times | Measure-Object -Average).Average
$p50 = ($times | Sort-Object)[50]
$p95 = ($times | Sort-Object)[95]
$p99 = ($times | Sort-Object)[99]

Write-Host "  Average: $($avg.ToString('F2')) ms" -ForegroundColor Green
Write-Host "  P50: $($p50.ToString('F2')) ms" -ForegroundColor Green
Write-Host "  P95: $($p95.ToString('F2')) ms" -ForegroundColor Green
Write-Host "  P99: $($p99.ToString('F2')) ms" -ForegroundColor Green
$Results += [PSCustomObject]@{Endpoint="/pay/refund/query"; Avg=$avg; P50=$p50; P95=$p95; P99=$p99}
Write-Host ""

# Test 4: Auth metrics endpoint
Write-Host "[4/4] Testing /pay/metrics/auth endpoint..." -ForegroundColor Yellow
$times = @()
for ($i = 0; $i -lt 100; $i++) {
    $start = Get-Date
    try {
        $response = Invoke-WebRequest -Uri "$BaseUrl/pay/metrics/auth" -Headers @{"X-Api-Key"="test-api-key"} -UseBasicParsing -TimeoutSec 5
        $end = Get-Date
        $duration = ($end - $start).TotalMilliseconds
        $times += $duration
    } catch {
        $times += 9999
    }
}

$avg = ($times | Measure-Object -Average).Average
$p50 = ($times | Sort-Object)[50]
$p95 = ($times | Sort-Object)[95]
$p99 = ($times | Sort-Object)[99]

Write-Host "  Average: $($avg.ToString('F2')) ms" -ForegroundColor Green
Write-Host "  P50: $($p50.ToString('F2')) ms" -ForegroundColor Green
Write-Host "  P95: $($p95.ToString('F2')) ms" -ForegroundColor Green
Write-Host "  P99: $($p99.ToString('F2')) ms" -ForegroundColor Green
$Results += [PSCustomObject]@{Endpoint="/pay/metrics/auth"; Avg=$avg; P50=$p50; P95=$p95; P99=$p99}
Write-Host ""

# Summary
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "Performance Test Summary" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
$Results | Format-Table -AutoSize

Write-Host ""
Write-Host "Performance Targets:" -ForegroundColor Yellow
Write-Host "  P50 < 50ms:   " -NoNewline
foreach ($r in $Results) {
    if ($r.P50 -lt 50) { Write-Host "✓" -ForegroundColor Green } else { Write-Host "✗" -ForegroundColor Red }
}

Write-Host "  P95 < 200ms:  " -NoNewline
foreach ($r in $Results) {
    if ($r.P95 -lt 200) { Write-Host "✓" -ForegroundColor Green } else { Write-Host "✗" -ForegroundColor Red }
}

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "Test complete!" -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Cyan
