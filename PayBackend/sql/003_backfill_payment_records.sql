-- Backfill payment records for orders that don't have them
-- This script creates payment records for existing orders

-- Insert payment records for orders that don't have corresponding payment records
INSERT INTO pay_payment (payment_no, order_no, status, amount, request_payload, response_payload, channel_trade_no, created_at, updated_at)
SELECT
    'PAY-' || o.order_no as payment_no,
    o.order_no,
    CASE
        WHEN o.status = 'PAID' THEN 'SUCCESS'
        WHEN o.status = 'FAILED' THEN 'FAILED'
        WHEN o.status = 'PAYING' THEN 'PROCESSING'
        ELSE 'INIT'
    END as status,
    o.amount,
    '{"created_by": "backfill_script"}' as request_payload,
    CASE
        WHEN o.status = 'PAID' THEN '{"status": "TRADE_SUCCESS", "backfilled": true}'::jsonb
        ELSE NULL
    END as response_payload,
    NULL as channel_trade_no,
    o.created_at,
    o.updated_at
FROM pay_order o
WHERE NOT EXISTS (
    SELECT 1 FROM pay_payment p WHERE p.order_no = o.order_no
)
AND o.status IN ('PAID', 'PAYING', 'FAILED');

-- Verify the backfill
SELECT
    o.order_no,
    o.status as order_status,
    p.payment_no,
    p.status as payment_status,
    p.amount
FROM pay_order o
LEFT JOIN pay_payment p ON o.order_no = p.order_no
ORDER BY o.created_at DESC
LIMIT 10;
