# Performance Test - Simple Version
$BaseUrl = "http://localhost:5566"
$ApiKey = "performance-test-key"

Write-Host "========================================"
Write-Host "Pay Plugin Performance Test"
Write-Host "========================================"
Write-Host ""

# Test 1: Metrics endpoint
Write-Host "[Test 1/4] GET /metrics (50 samples)"
$times = @()
for ($i = 0; $i -lt 50; $i++) {
    $start = Get-Date
    try {
        $null = Invoke-WebRequest -Uri "$BaseUrl/metrics" -UseBasicParsing -TimeoutSec 5
        $end = Get-Date
        $times += (($end - $start).TotalMilliseconds)
    } catch {
        $times += 9999
    }
}
$metrics_avg = ($times | Measure-Object -Average).Average
$metrics_p50 = ($times | Sort-Object)[24]
$metrics_p95 = ($times | Sort-Object)[47]
$metrics_p99 = ($times | Sort-Object)[48]
Write-Host "  Average: $([math]::Round($metrics_avg, 2)) ms"
Write-Host "  P50: $([math]::Round($metrics_p50, 2)) ms"
Write-Host "  P95: $([math]::Round($metrics_p95, 2)) ms"
Write-Host "  P99: $([math]::Round($metrics_p99, 2)) ms"
Write-Host ""

# Test 2: Query order endpoint
Write-Host "[Test 2/4] GET /pay/query (50 samples)"
$times = @()
for ($i = 0; $i -lt 50; $i++) {
    $start = Get-Date
    try {
        $response = Invoke-WebRequest -Uri "$BaseUrl/pay/query?order_no=test_$i" -Headers @{"X-Api-Key"=$ApiKey} -UseBasicParsing -TimeoutSec 5
        $end = Get-Date
        $times += (($end - $start).TotalMilliseconds)
    } catch {
        $times += 9999
    }
}
$query_avg = ($times | Measure-Object -Average).Average
$query_p50 = ($times | Sort-Object)[24]
$query_p95 = ($times | Sort-Object)[47]
$query_p99 = ($times | Sort-Object)[48]
Write-Host "  Average: $([math]::Round($query_avg, 2)) ms"
Write-Host "  P50: $([math]::Round($query_p50, 2)) ms"
Write-Host "  P95: $([math]::Round($query_p95, 2)) ms"
Write-Host "  P99: $([math]::Round($query_p99, 2)) ms"
Write-Host ""

# Test 3: Auth metrics endpoint
Write-Host "[Test 3/4] GET /pay/metrics/auth (50 samples)"
$times = @()
for ($i = 0; $i -lt 50; $i++) {
    $start = Get-Date
    try {
        $response = Invoke-WebRequest -Uri "$BaseUrl/pay/metrics/auth" -Headers @{"X-Api-Key"=$ApiKey} -UseBasicParsing -TimeoutSec 5
        $end = Get-Date
        $times += (($end - $start).TotalMilliseconds)
    } catch {
        $times += 9999
    }
}
$auth_avg = ($times | Measure-Object -Average).Average
$auth_p50 = ($times | Sort-Object)[24]
$auth_p95 = ($times | Sort-Object)[47]
$auth_p99 = ($times | Sort-Object)[48]
Write-Host "  Average: $([math]::Round($auth_avg, 2)) ms"
Write-Host "  P50: $([math]::Round($auth_p50, 2)) ms"
Write-Host "  P95: $([math]::Round($auth_p95, 2)) ms"
Write-Host "  P99: $([math]::Round($auth_p99, 2)) ms"
Write-Host ""

# Test 4: Refund query endpoint
Write-Host "[Test 4/4] GET /pay/refund/query (50 samples)"
$times = @()
for ($i = 0; $i -lt 50; $i++) {
    $start = Get-Date
    try {
        $response = Invoke-WebRequest -Uri "$BaseUrl/pay/refund/query?refund_no=test_$i" -Headers @{"X-Api-Key"=$ApiKey} -UseBasicParsing -TimeoutSec 5
        $end = Get-Date
        $times += (($end - $start).TotalMilliseconds)
    } catch {
        $times += 9999
    }
}
$refund_avg = ($times | Measure-Object -Average).Average
$refund_p50 = ($times | Sort-Object)[24]
$refund_p95 = ($times | Sort-Object)[47]
$refund_p99 = ($times | Sort-Object)[48]
Write-Host "  Average: $([math]::Round($refund_avg, 2)) ms"
Write-Host "  P50: $([math]::Round($refund_p50, 2)) ms"
Write-Host "  P95: $([math]::Round($refund_p95, 2)) ms"
Write-Host "  P99: $([math]::Round($refund_p99, 2)) ms"
Write-Host ""

# Summary table
Write-Host "========================================"
Write-Host "Performance Summary"
Write-Host "========================================"
Write-Host "Endpoint              Avg    P50    P95    P99"
Write-Host "--------              ---    ---    ---    ---"
Write-Host "/metrics             $([math]::Round($metrics_avg, 5))    $([math]::Round($metrics_p50, 5))    $([math]::Round($metrics_p95, 5))    $([math]::Round($metrics_p99, 5))"
Write-Host "/pay/query           $([math]::Round($query_avg, 5))    $([math]::Round($query_p50, 5))    $([math]::Round($query_p95, 5))    $([math]::Round($query_p99, 5))"
Write-Host "/pay/metrics/auth    $([math]::Round($auth_avg, 5))    $([math]::Round($auth_p50, 5))    $([math]::Round($auth_p95, 5))    $([math]::Round($auth_p99, 5))"
Write-Host "/pay/refund/query     $([math]::Round($refund_avg, 5))    $([math]::Round($refund_p50, 5))    $([math]::Round($refund_p95, 5))    $([math]::Round($refund_p99, 5))"
Write-Host "========================================"
Write-Host ""

# Performance targets
Write-Host "Performance Targets:"
Write-Host "  Target: P50 < 50ms"
if ($metrics_p50 -lt 50) { Write-Host "  /metrics: PASS" -ForegroundColor Green } else { Write-Host "  /metrics: FAIL" -ForegroundColor Red }
if ($query_p50 -lt 50) { Write-Host "  /pay/query: PASS" -ForegroundColor Green } else { Write-Host "  /pay/query: FAIL" -ForegroundColor Red }
if ($auth_p50 -lt 50) { Write-Host "  /pay/metrics/auth: PASS" -ForegroundColor Green } else { Write-Host "  /pay/metrics/auth: FAIL" -ForegroundColor Red }
if ($refund_p50 -lt 50) { Write-Host "  /pay/refund/query: PASS" -ForegroundColor Green } else { Write-Host "  /pay/refund/query: FAIL" -ForegroundColor Red }
Write-Host ""
Write-Host "  Target: P95 < 200ms"
if ($metrics_p95 -lt 200) { Write-Host "  /metrics: PASS" -ForegroundColor Green } else { Write-Host "  /metrics: FAIL" -ForegroundColor Red }
if ($query_p95 -lt 200) { Write-Host "  /pay/query: PASS" -ForegroundColor Green } else { Write-Host "  /pay/query: FAIL" -ForegroundColor Red }
if ($auth_p95 -lt 200) { Write-Host "  /pay/metrics/auth: PASS" -ForegroundColor Green } else { Write-Host "  /pay/metrics/auth: FAIL" -ForegroundColor Red }
if ($refund_p95 -lt 200) { Write-Host "  /pay/refund/query: PASS" -ForegroundColor Green } else { Write-Host "  /pay/refund/query: FAIL" -ForegroundColor Red }

Write-Host ""
Write-Host "========================================"
Write-Host "Test complete!"
Write-Host "========================================"
