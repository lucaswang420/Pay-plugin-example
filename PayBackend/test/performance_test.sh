#!/bin/bash
# =============================================================================
# Pay Plugin Performance Test Script
# =============================================================================
# Tests API performance using current (not documented) API format
# =============================================================================

set -e

BASE_URL="${BASE_URL:-http://localhost:5566}"
RESULTS_DIR="performance_results_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$RESULTS_DIR"

echo "============================================="
echo "Pay Plugin Performance Test"
echo "============================================="
echo "Base URL: $BASE_URL"
echo "Results: $RESULTS_DIR"
echo "============================================="
echo ""

# Check if ab (Apache Bench) is available
if ! command -v ab &> /dev/null; then
    echo "Error: Apache Bench (ab) not found"
    echo "Please install: sudo apt-get install apache2-utils"
    exit 1
fi

# =============================================================================
# Test 1: Metrics Endpoint (Lightweight)
# =============================================================================
echo "[1/4] Testing /metrics endpoint..."
ab -n 1000 -c 10 -g "$RESULTS_DIR/metrics.tsv" \
   "$BASE_URL/metrics" > "$RESULTS_DIR/metrics.txt" 2>&1

echo "✓ Metrics endpoint test complete"
echo ""

# =============================================================================
# Test 2: Query Order (Read Operation)
# =============================================================================
echo "[2/4] Testing query order endpoint..."
TEST_ORDER_NO="test_order_$(date +%s)"

ab -n 1000 -c 10 \
   "$BASE_URL/pay/query?order_no=$TEST_ORDER_NO" \
   > "$RESULTS_DIR/query_order.txt" 2>&1

echo "✓ Query order test complete"
echo ""

# =============================================================================
# Test 3: Query Refund (Read Operation)
# =============================================================================
echo "[3/4] Testing query refund endpoint..."
TEST_REFUND_NO="test_refund_$(date +%s)"

ab -n 1000 -c 10 \
   "$BASE_URL/pay/refund/query?refund_no=$TEST_REFUND_NO" \
   > "$RESULTS_DIR/query_refund.txt" 2>&1

echo "✓ Query refund test complete"
echo ""

# =============================================================================
# Test 4: Auth Metrics (Read Operation)
# =============================================================================
echo "[4/4] Testing auth metrics endpoint..."
ab -n 1000 -c 10 \
   -H "X-Api-Key: test-api-key" \
   "$BASE_URL/pay/metrics/auth" \
   > "$RESULTS_DIR/auth_metrics.txt" 2>&1

echo "✓ Auth metrics test complete"
echo ""

# =============================================================================
# Generate Summary Report
# =============================================================================
echo "============================================="
echo "Performance Test Summary"
echo "============================================="
echo ""

for test in metrics query_order query_refund auth_metrics; do
    file="$RESULTS_DIR/${test}.txt"
    if [ -f "$file" ]; then
        echo "=== $test ==="
        grep -E "Requests per second|Time per request|Transfer rate" "$file" || echo "No metrics found"
        echo ""
    fi
done

echo "============================================="
echo "Results saved to: $RESULTS_DIR"
echo "============================================="
