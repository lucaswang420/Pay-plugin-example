# Simple Performance Test - No Colors
$BaseUrl = "http://localhost:5566"
$ApiKey = "performance-test-key"

Write-Host "========================================"
Write-Host "Pay Plugin Performance Test"
Write-Host "========================================"
Write-Host ""

# Test 1: Metrics endpoint
Write-Host "[Test 1/3] GET /metrics (100 samples)"
$times = @()
for ($i = 0; $i -lt 100; $i++) {
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
$metrics_p50 = ($times | Sort-Object)[49]
$metrics_p95 = ($times | Sort-Object)[94]
$metrics_p99 = ($times | Sort-Object)[98]
Write-Host "  Average: $([math]::Round($metrics_avg, 2)) ms"
Write-Host "  P50: $([math]::Round($metrics_p50, 2)) ms"
Write-Host "  P95: $([math]::Round($metrics_p95, 2)) ms"
Write-Host "  P99: $([math]::Round($metrics_p99, 2)) ms"
Write-Host ""

# Test 2: Query order endpoint
Write-Host "[Test 2/3] GET /pay/query (100 samples)"
$times = @()
for ($i = 0; $i -lt 100; $i++) {
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
$query_p50 = ($times | Sort-Object)[49]
$query_p95 = ($times | Sort-Object)[94]
$query_p99 = ($times | Sort-Object)[98]
Write-Host "  Average: $([math]::Round($query_avg, 2)) ms"
Write-Host "  P50: $([math]::Round($query_p50, 2)) ms"
Write-Host "  P95: $([math]::Round($query_p95, 2)) ms"
Write-Host "  P99: $([math]::Round($query_p99, 2)) ms"
Write-Host ""

# Test 3: Auth metrics endpoint
Write-Host "[Test 3/3] GET /pay/metrics/auth (100 samples)"
$times = @()
for ($i = 0; $i -lt 100; $i++) {
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
$auth_p50 = ($times | Sort-Object)[49]
$auth_p95 = ($times | Sort-Object)[94]
$auth_p99 = ($times | Sort-Object)[98]
Write-Host "  Average: $([math]::Round($auth_avg, 2)) ms"
Write-Host "  P50: $([math]::Round($auth_p50, 2)) ms"
Write-Host "  P95: $([math]::Round($auth_p95, 2)) ms"
Write-Host "  P99: $([math]::Round($auth_p99, 2)) ms"
Write-Host ""

# Calculate throughput
$metrics_rps = [math]::Round(1000 / $metrics_avg, 0)
$query_rps = [math]::Round(1000 / $query_avg, 0)
$auth_rps = [math]::Round(1000 / $auth_avg, 0)

# Summary
Write-Host "========================================"
Write-Host "Performance Test Summary"
Write-Host "========================================"
Write-Host ""
Write-Host "Endpoint              Avg    P50    P95    P99    RPS"
Write-Host "--------              ---    ---    ---    ---    ---"
Write-Host "/metrics             $([math]::Round($metrics_avg, 5))    $([math]::Round($metrics_p50, 5))    $([math]::Round($metrics_p95, 5))    $([math]::Round($metrics_p99, 5))    $metrics_rps"
Write-Host "/pay/query           $([math]::Round($query_avg, 5))    $([math]::Round($query_p50, 5))    $([math]::Round($query_p95, 5))    $([math]::Round($query_p99, 5))    $query_rps"
Write-Host "/pay/metrics/auth    $([math]::Round($auth_avg, 5))    $([math]::Round($auth_p50, 5))    $([math]::Round($auth_p95, 5))    $([math]::Round($auth_p99, 5))    $auth_rps"
Write-Host "========================================"
Write-Host ""

# Performance targets
Write-Host "Performance Targets:"
Write-Host "  Target: P50 < 50ms"
if ($metrics_p50 -lt 50) { Write-Host "  /metrics: PASS" } else { Write-Host "  /metrics: FAIL" }
if ($query_p50 -lt 50) { Write-Host "  /pay/query: PASS" } else { Write-Host "  /pay/query: FAIL" }
if ($auth_p50 -lt 50) { Write-Host "  /pay/metrics/auth: PASS" } else { Write-Host "  /pay/metrics/auth: FAIL" }
Write-Host "  Target: P95 < 200ms"
if ($metrics_p95 -lt 200) { Write-Host "  /metrics: PASS" } else { Write-Host "  /metrics: FAIL" }
if ($query_p95 -lt 200) { Write-Host "  /pay/query: PASS" } else { Write-Host "  /pay/query: FAIL" }
if ($auth_p95 -lt 200) { Write-Host "  /pay/metrics/auth: PASS" } else { Write-Host "  /pay/metrics/auth: FAIL" }
Write-Host "  Target: Throughput >= 100 RPS"
if ($metrics_rps -ge 100) { Write-Host "  /metrics: PASS" } else { Write-Host "  /metrics: FAIL" }
if ($query_rps -ge 100) { Write-Host "  /pay/query: PASS" } else { Write-Host "  /pay/query: FAIL" }
if ($auth_rps -ge 100) { Write-Host "  /pay/metrics/auth: PASS" } else { Write-Host "  /pay/metrics/auth: FAIL" }

Write-Host ""
Write-Host "========================================"
Write-Host "Test complete!"
Write-Host "========================================"
