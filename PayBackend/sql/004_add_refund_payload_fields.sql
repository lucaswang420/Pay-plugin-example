-- Add missing fields to pay_refund table
-- Migration script to add request_payload and response_payload fields

ALTER TABLE pay_refund ADD COLUMN IF NOT EXISTS request_payload TEXT;
ALTER TABLE pay_refund ADD COLUMN IF NOT EXISTS response_payload TEXT;

-- Verify the columns were added
SELECT column_name, data_type
FROM information_schema.columns
WHERE table_name = 'pay_refund'
AND column_name IN ('request_payload', 'response_payload');
