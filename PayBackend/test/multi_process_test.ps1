# Multi-process concurrent test
$BaseUrl = "http://localhost:5566"
$ApiKey = "performance-test-key"
$Samples = 50
$Processes = 4  # 4个并发进程

Write-Host "========================================"
Write-Host "Multi-Process Concurrent Test"
Write-Host "========================================"
Write-Host "Processes: $Processes (simulating concurrent clients)"
Write-Host "Samples per process: $Samples"
Write-Host "Total requests: $($Samples * $Processes)"
Write-Host ""

# 创建测试脚本块
$testScript = {
    param($url, $key, $count, $processId)

    $success = 0
    $errors = 0
    $times = @()

    for ($i = 0; $i -lt $count; $i++) {
        $start = Get-Date
        try {
            $response = Invoke-WebRequest -Uri "$url&proc=$processId&req=$i" -Headers @{"X-Api-Key"=$key} -UseBasicParsing -TimeoutSec 10
            $end = Get-Date
            $duration = ($end - $start).TotalMilliseconds
            $times += $duration
            if ($response.StatusCode -eq 200) {
                $success++
            }
        } catch {
            $errors++
            $times += 9999
        }
    }

    return @{
        success = $success
        errors = $errors
        times = $times
        avg = ($times | Measure-Object -Average).Average
        p50 = ($times | Sort-Object)[([int]($count * 0.5))]
        p95 = ($times | Sort-Object)[([int]($count * 0.95))]
        p99 = ($times | Sort-Object)[([int]($count * 0.99))]
    }
}

# Test 1: /pay/query
Write-Host "[Test 1/2] GET /pay/query ($Processes processes x $Samples requests)"

$jobs = @()
$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

for ($p = 0; $p -lt $Processes; $p++) {
    $job = Start-Job -ScriptBlock $testScript -ArgumentList "$BaseUrl/pay/query?order_no=test", $ApiKey, $Samples, $p
    $jobs += $job
}

# 等待所有作业完成
$allResults = @()
foreach ($job in $jobs) {
    $result = Receive-Job -Job $job -Wait
    $allResults += $result
    Remove-Job -Job $job
}

$stopwatch.Stop()

# 汇总结果
$totalSuccess = ($allResults | ForEach-Object { $_.success } | Measure-Object -Sum).Sum
$totalErrors = ($allResults | ForEach-Object { $_.errors } | Measure-Object -Sum).Sum
$allTimes = @()
foreach ($result in $allResults) {
    $allTimes += $result.times
}

$totalSeconds = $stopwatch.Elapsed.TotalSeconds
$totalRequests = $Samples * $Processes
$actualRPS = [math]::Round($totalRequests / $totalSeconds, 2)

$avg = ($allTimes | Measure-Object -Average).Average
$p50 = ($allTimes | Sort-Object)[([int]($totalRequests * 0.5))]
$p95 = ($allTimes | Sort-Object)[([int]($totalRequests * 0.95))]
$p99 = ($allTimes | Sort-Object)[([int]($totalRequests * 0.99))]

Write-Host ""
Write-Host "Results:"
Write-Host "  Total time: $([math]::Round($totalSeconds, 2)) seconds"
Write-Host "  Total requests: $totalRequests"
Write-Host "  Success: $totalSuccess"
Write-Host "  Errors: $totalErrors"
Write-Host "  Average: $([math]::Round($avg, 2)) ms"
Write-Host "  P50: $([math]::Round($p50, 2)) ms"
Write-Host "  P95: $([math]::Round($p95, 2)) ms"
Write-Host "  P99: $([math]::Round($p99, 2)) ms"
Write-Host "  ** Actual throughput: $actualRPS RPS **"
Write-Host ""

# Test 2: /pay/metrics/auth
Write-Host "[Test 2/2] GET /pay/metrics/auth ($Processes processes x $Samples requests)"

$jobs = @()
$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

for ($p = 0; $p -lt $Processes; $p++) {
    $job = Start-Job -ScriptBlock $testScript -ArgumentList "$BaseUrl/pay/metrics/auth", $ApiKey, $Samples, $p
    $jobs += $job
}

$allResults = @()
foreach ($job in $jobs) {
    $result = Receive-Job -Job $job -Wait
    $allResults += $result
    Remove-Job -Job $job
}

$stopwatch.Stop()

$totalSuccess = ($allResults | ForEach-Object { $_.success } | Measure-Object -Sum).Sum
$totalErrors = ($allResults | ForEach-Object { $_.errors } | Measure-Object -Sum).Sum
$allTimes = @()
foreach ($result in $allResults) {
    $allTimes += $result.times
}

$totalSeconds = $stopwatch.Elapsed.TotalSeconds
$totalRequests = $Samples * $Processes
$actualRPS = [math]::Round($totalRequests / $totalSeconds, 2)

$avg = ($allTimes | Measure-Object -Average).Average
$p50 = ($allTimes | Sort-Object)[([int]($totalRequests * 0.5))]
$p95 = ($allTimes | Sort-Object)[([int]($totalRequests * 0.95))]
$p99 = ($allTimes | Sort-Object)[([int]($totalRequests * 0.99))]

Write-Host ""
Write-Host "Results:"
Write-Host "  Total time: $([math]::Round($totalSeconds, 2)) seconds"
Write-Host "  Total requests: $totalRequests"
Write-Host "  Success: $totalSuccess"
Write-Host "  Errors: $totalErrors"
Write-Host "  Average: $([math]::Round($avg, 2)) ms"
Write-Host "  P50: $([math]::Round($p50, 2)) ms"
Write-Host "  P95: $([math]::Round($p95, 2)) ms"
Write-Host "  P99: $([math]::Round($p99, 2)) ms"
Write-Host "  ** Actual throughput: $actualRPS RPS **"
Write-Host ""

Write-Host "========================================"
Write-Host "Summary"
Write-Host "========================================"
Write-Host ""
Write-Host "This test uses $Processes concurrent processes to"
Write-Host "simulate real concurrent load on the server."
Write-Host ""
Write-Host "With 4 server threads, we expect to see:"
Write-Host "  - Throughput significantly > 100 RPS"
Write-Host "  - Response times remain stable"
Write-Host ""
Write-Host "If RPS > 100, optimization is successful!"
Write-Host ""
