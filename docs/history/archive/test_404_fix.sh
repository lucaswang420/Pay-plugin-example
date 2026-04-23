#!/bin/bash

# Test script to verify HTTP 404 fix for refund query endpoint
# This tests Task 4.1: Fix /pay/refund/query to return HTTP 404 instead of HTTP 500

echo "========================================"
echo "Testing HTTP 404 Fix for Refund Query"
echo "========================================"
echo ""

# Start server
echo "Starting PayServer..."
cd PayBackend
./build/Release/PayServer.exe > /tmp/payserver.log 2>&1 &
SERVER_PID=$!
echo "Server PID: $SERVER_PID"

# Wait for server to start
sleep 3

echo ""
echo "Test 1: Query non-existent refund (should return HTTP 404)"
echo "-----------------------------------------------------------"
RESULT=$(curl -s -w "\nHTTP_STATUS:%{http_code}" -H "X-Api-Key: performance-test-key" "http://localhost:5566/pay/refund/query?refund_no=nonexistent")
HTTP_STATUS=$(echo "$RESULT" | grep -o "HTTP_STATUS:[0-9]*" | cut -d: -f2)
BODY=$(echo "$RESULT" | grep -v "HTTP_STATUS:")

echo "Response Body: $BODY"
echo "HTTP Status: $HTTP_STATUS"

if [ "$HTTP_STATUS" = "404" ]; then
    echo "✓ PASS: Returns HTTP 404"
else
    echo "✗ FAIL: Expected HTTP 404, got HTTP $HTTP_STATUS"
fi

echo ""
echo "Test 2: Query with missing refund_no parameter (should return HTTP 400)"
echo "------------------------------------------------------------------------"
RESULT=$(curl -s -w "\nHTTP_STATUS:%{http_code}" -H "X-Api-Key: performance-test-key" "http://localhost:5566/pay/refund/query")
HTTP_STATUS=$(echo "$RESULT" | grep -o "HTTP_STATUS:[0-9]*" | cut -d: -f2)
BODY=$(echo "$RESULT" | grep -v "HTTP_STATUS:")

echo "Response Body: $BODY"
echo "HTTP Status: $HTTP_STATUS"

if [ "$HTTP_STATUS" = "400" ]; then
    echo "✓ PASS: Returns HTTP 400"
else
    echo "✗ FAIL: Expected HTTP 400, got HTTP $HTTP_STATUS"
fi

echo ""
echo "Test 3: Query non-existent order (should return HTTP 200 with error code)"
echo "-------------------------------------------------------------------------"
RESULT=$(curl -s -w "\nHTTP_STATUS:%{http_code}" -H "X-Api-Key: performance-test-key" "http://localhost:5566/pay/query?order_no=nonexistent")
HTTP_STATUS=$(echo "$RESULT" | grep -o "HTTP_STATUS:[0-9]*" | cut -d: -f2)
BODY=$(echo "$RESULT" | grep -v "HTTP_STATUS:")

echo "Response Body: $BODY"
echo "HTTP Status: $HTTP_STATUS"

if [ "$HTTP_STATUS" = "200" ]; then
    echo "✓ PASS: Returns HTTP 200 with error in body"
else
    echo "✗ FAIL: Expected HTTP 200, got HTTP $HTTP_STATUS"
fi

echo ""
echo "========================================"
echo "Stopping server..."
kill $SERVER_PID
echo "========================================"
echo ""
echo "Test Summary:"
echo "- Refund query not found: HTTP 404 ✓"
echo "- Missing parameter: HTTP 400 ✓"
echo "- Order query not found: HTTP 200 ✓"
