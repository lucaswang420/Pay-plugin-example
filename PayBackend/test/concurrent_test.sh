#!/bin/bash
# Simple concurrent test using curl
BASE_URL="http://localhost:5566"
API_KEY="performance-test-key"
SAMPLES=100
CONCURRENT=4

echo "========================================"
echo "Concurrent Performance Test"
echo "========================================"
echo "Concurrent: $CONCURRENT"
echo "Samples: $SAMPLES"
echo ""

# Test 1: /pay/query
echo "[Test 1/2] GET /pay/query"
echo "Running $SAMPLES requests with $CONCURRENT concurrent..."

START_TIME=$(date +%s.%N)

for ((i=0; i<SAMPLES; i++)); do
  # 启动并发请求
  for ((j=0; j<CONCURRENT; j++)); do
    (
      curl -s -w "\n%{http_code}\n" \
        -H "X-Api-Key: $API_KEY" \
        "$BASE_URL/pay/query?order_no=test_${i}_${j}" \
        > /dev/null 2>&1
    ) &
  done

  # 等待所有后台任务完成
  wait

  if (( $i % 25 == 0 )); then
    echo "  Progress: $i/$SAMPLES"
  fi
done

END_TIME=$(date +%s.%N)
TOTAL_TIME=$(awk "BEGIN {print $END_TIME - $START_TIME}")
TOTAL_REQUESTS=$((SAMPLES * CONCURRENT))
RPS=$(awk "BEGIN {printf \"%.2f\", $TOTAL_REQUESTS / $TOTAL_TIME}")

echo ""
echo "Results:"
echo "  Total time: $(printf "%.2f" $TOTAL_TIME) seconds"
echo "  Total requests: $TOTAL_REQUESTS"
echo "  Throughput: $(printf "%.2f" $RPS) RPS"
echo ""

# Test 2: /pay/metrics/auth
echo "[Test 2/2] GET /pay/metrics/auth"
echo "Running $SAMPLES requests with $CONCURRENT concurrent..."

START_TIME=$(date +%s.%N)

for ((i=0; i<SAMPLES; i++)); do
  for ((j=0; j<CONCURRENT; j++)); do
    (
      curl -s -w "\n%{http_code}\n" \
        -H "X-Api-Key: $API_KEY" \
        "$BASE_URL/pay/metrics/auth" \
        > /dev/null 2>&1
    ) &
  done
  wait

  if (( $i % 25 == 0 )); then
    echo "  Progress: $i/$SAMPLES"
  fi
done

END_TIME=$(date +%s.%N)
TOTAL_TIME=$(awk "BEGIN {print $END_TIME - $START_TIME}")
TOTAL_REQUESTS=$((SAMPLES * CONCURRENT))
RPS=$(awk "BEGIN {printf \"%.2f\", $TOTAL_REQUESTS / $TOTAL_TIME}")

echo ""
echo "Results:"
echo "  Total time: $(printf "%.2f" $TOTAL_TIME) seconds"
echo "  Total requests: $TOTAL_REQUESTS"
echo "  Throughput: $(printf "%.2f" $RPS) RPS"
echo ""

echo "========================================"
echo "Summary"
echo "========================================"
echo ""
echo "With 4 server threads and concurrent requests,"
echo "we expect throughput > 100 RPS"
echo ""
