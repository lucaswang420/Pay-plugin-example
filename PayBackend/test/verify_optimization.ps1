# Simple Concurrent Test - Multiple rapid requests
$BaseUrl = "http://localhost:5566"
$ApiKey = "performance-test-key"
$Samples = 100

Write-Host "========================================"
Write-Host "Optimization Verification Test"
Write-Host "========================================"
Write-Host "Config: 4 threads, 32 DB connections"
Write-Host "Test: Rapid sequential requests (100 each)"
Write-Host ""

# Test 1: /pay/query (fastest endpoint, best for testing)
Write-Host "[Test] GET /pay/query ($Samples samples)"

$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
$successCount = 0
$errorCount = 0

for ($i = 0; $i -lt $Samples; $i++) {
    try {
        $response = Invoke-WebRequest -Uri "$BaseUrl/pay/query?order_no=test_$i" -Headers @{"X-Api-Key"=$ApiKey} -UseBasicParsing -TimeoutSec 5
        if ($response.StatusCode -eq 200) {
            $successCount++
        }
    } catch {
        $errorCount++
    }

    if (($i + 1) % 10 -eq 0) {
        Write-Host "  Progress: $($i + 1)/$Samples"
    }
}

$stopwatch.Stop()

$totalSeconds = $stopwatch.Elapsed.TotalSeconds
$actualRPS = [math]::Round($Samples / $totalSeconds, 2)
$avgTime = ($totalSeconds * 1000) / $Samples

Write-Host ""
Write-Host "Results:"
Write-Host "  Total time: $([math]::Round($totalSeconds, 2)) seconds"
Write-Host "  Success: $successCount"
Write-Host "  Errors: $errorCount"
Write-Host "  Average per request: $([math]::Round($avgTime, 2)) ms"
Write-Host "  ** Actual throughput: $actualRPS RPS **"
Write-Host ""

# Test 2: /pay/metrics/auth (another fast endpoint)
Write-Host "[Test] GET /pay/metrics/auth ($Samples samples)"

$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
$successCount = 0
$errorCount = 0

for ($i = 0; $i -lt $Samples; $i++) {
    try {
        $response = Invoke-WebRequest -Uri "$BaseUrl/pay/metrics/auth" -Headers @{"X-Api-Key"=$ApiKey} -UseBasicParsing -TimeoutSec 5
        if ($response.StatusCode -eq 200) {
            $successCount++
        }
    } catch {
        $errorCount++
    }

    if (($i + 1) % 10 -eq 0) {
        Write-Host "  Progress: $($i + 1)/$Samples"
    }
}

$stopwatch.Stop()

$totalSeconds = $stopwatch.Elapsed.TotalSeconds
$actualRPS = [math]::Round($Samples / $totalSeconds, 2)
$avgTime = ($totalSeconds * 1000) / $Samples

Write-Host ""
Write-Host "Results:"
Write-Host "  Total time: $([math]::Round($totalSeconds, 2)) seconds"
Write-Host "  Success: $successCount"
Write-Host "  Errors: $errorCount"
Write-Host "  Average per request: $([math]::Round($avgTime, 2)) ms"
Write-Host "  ** Actual throughput: $actualRPS RPS **"
Write-Host ""

Write-Host "========================================"
Write-Host "Analysis:"
Write-Host "========================================"
Write-Host ""
Write-Host "Note: This test sends requests rapidly in sequence."
Write-Host "With 4 threads, the server can handle multiple requests concurrently."
Write-Host "If throughput > 100 RPS, the optimization is successful!"
Write-Host ""
Write-Host "Target: >= 100 RPS"
Write-Host "Actual: Check results above"
Write-Host ""
