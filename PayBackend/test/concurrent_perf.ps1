# Concurrent Performance Test - Tests multi-threading capability
$BaseUrl = "http://localhost:5566"
$ApiKey = "performance-test-key"
$ConcurrentRequests = 10  # 同时发送10个请求
$Samples = 100  # 总共100个样本

Write-Host "========================================"
Write-Host "Pay Plugin Concurrent Performance Test"
Write-Host "========================================"
Write-Host "Configuration: 4 threads, 32 DB connections"
Write-Host "Test: Concurrent requests to measure throughput"
Write-Host ""

# Test 1: Metrics endpoint
Write-Host "[Test 1/3] GET /metrics (100 samples, concurrent)"
$times = @()
$batches = [math]::Ceiling($Samples / $ConcurrentRequests)

for ($b = 0; $b -lt $batches; $b++) {
    $batchSize = [Math]::Min($ConcurrentRequests, $Samples - ($b * $ConcurrentRequests))

    # 创建并发任务
    $tasks = @()
    $jobs = @()

    for ($i = 0; $i -lt $batchSize; $i++) {
        $scriptBlock = {
            param($url)
            $start = Get-Date
            try {
                $null = Invoke-WebRequest -Uri $url -UseBasicParsing -TimeoutSec 5
                $end = Get-Date
                return ($end - $start).TotalMilliseconds
            } catch {
                return 9999
            }
        }
        $jobs += Start-Job -ScriptBlock $scriptBlock -ArgumentList "$BaseUrl/metrics"
    }

    # 等待所有任务完成并收集结果
    foreach ($job in $jobs) {
        $result = Receive-Job -Job $job -Wait
        $times += $result
        Remove-Job -Job $job
    }

    $progress = (($b + 1) * $batchSize)
    Write-Host "  Progress: $progress/$Samples samples completed"
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
Write-Host "[Test 2/3] GET /pay/query (100 samples, concurrent)"
$times = @()
$batches = [math]::Ceiling($Samples / $ConcurrentRequests)

for ($b = 0; $b -lt $batches; $b++) {
    $batchSize = [Math]::Min($ConcurrentRequests, $Samples - ($b * $ConcurrentRequests))

    $jobs = @()
    for ($i = 0; $i -lt $batchSize; $i++) {
        $scriptBlock = {
            param($url, $key)
            $start = Get-Date
            try {
                $response = Invoke-WebRequest -Uri $url -Headers @{"X-Api-Key"=$key} -UseBasicParsing -TimeoutSec 5
                $end = Get-Date
                return ($end - $start).TotalMilliseconds
            } catch {
                return 9999
            }
        }
        $jobs += Start-Job -ScriptBlock $scriptBlock -ArgumentList "$BaseUrl/pay/query?order_no=test_$b`_$i", $ApiKey
    }

    foreach ($job in $jobs) {
        $result = Receive-Job -Job $job -Wait
        $times += $result
        Remove-Job -Job $job
    }

    $progress = (($b + 1) * $batchSize)
    Write-Host "  Progress: $progress/$Samples samples completed"
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
Write-Host "[Test 3/3] GET /pay/metrics/auth (100 samples, concurrent)"
$times = @()
$batches = [math]::Ceiling($Samples / $ConcurrentRequests)

for ($b = 0; $b -lt $batches; $b++) {
    $batchSize = [Math]::Min($ConcurrentRequests, $Samples - ($b * $ConcurrentRequests))

    $jobs = @()
    for ($i = 0; $i -lt $batchSize; $i++) {
        $scriptBlock = {
            param($url, $key)
            $start = Get-Date
            try {
                $response = Invoke-WebRequest -Uri $url -Headers @{"X-Api-Key"=$key} -UseBasicParsing -TimeoutSec 5
                $end = Get-Date
                return ($end - $start).TotalMilliseconds
            } catch {
                return 9999
            }
        }
        $jobs += Start-Job -ScriptBlock $scriptBlock -ArgumentList "$BaseUrl/pay/metrics/auth", $ApiKey
    }

    foreach ($job in $jobs) {
        $result = Receive-Job -Job $job -Wait
        $times += $result
        Remove-Job -Job $job
    }

    $progress = (($b + 1) * $batchSize)
    Write-Host "  Progress: $progress/$Samples samples completed"
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

# Calculate throughput (考虑并发)
$metrics_rps = [math]::Round(1000 / $metrics_avg * $ConcurrentRequests, 0)
$query_rps = [math]::Round(1000 / $query_avg * $ConcurrentRequests, 0)
$auth_rps = [math]::Round(1000 / $auth_avg * $ConcurrentRequests, 0)

# Summary
Write-Host "========================================"
Write-Host "Concurrent Performance Test Summary"
Write-Host "========================================"
Write-Host ""
Write-Host "Endpoint              Avg    P50    P95    P99    RPS (est.)"
Write-Host "--------              ---    ---    ---    ---    ---------"
Write-Host "/metrics             $([math]::Round($metrics_avg, 5))    $([math]::Round($metrics_p50, 5))    $([math]::Round($metrics_p95, 5))    $([math]::Round($metrics_p99, 5))    $metrics_rps"
Write-Host "/pay/query           $([math]::Round($query_avg, 5))    $([math]::Round($query_p50, 5))    $([math]::Round($query_p95, 5))    $([math]::Round($query_p99, 5))    $query_rps"
Write-Host "/pay/metrics/auth    $([math]::Round($auth_avg, 5))    $([math]::Round($auth_p50, 5))    $([math]::Round($auth_p95, 5))    $([math]::Round($auth_p99, 5))    $auth_rps"
Write-Host "========================================"
Write-Host ""
Write-Host "Note: RPS estimated based on $ConcurrentRequests concurrent requests"
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
if ($metrics_rps -ge 100) { Write-Host "  /metrics: PASS ($metrics_rps RPS)" } else { Write-Host "  /metrics: FAIL ($metrics_rps RPS)" }
if ($query_rps -ge 100) { Write-Host "  /pay/query: PASS ($query_rps RPS)" } else { Write-Host "  /pay/query: FAIL ($query_rps RPS)" }
if ($auth_rps -ge 100) { Write-Host "  /pay/metrics/auth: PASS ($auth_rps RPS)" } else { Write-Host "  /pay/metrics/auth: FAIL ($auth_rps RPS)" }

Write-Host ""
Write-Host "========================================"
Write-Host "Test complete!"
Write-Host "========================================"
