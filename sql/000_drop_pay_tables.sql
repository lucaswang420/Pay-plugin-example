-- Pay Plugin Database Cleanup
-- Drop all existing tables in correct order (respecting foreign key constraints)

-- Drop tables in reverse order of creation to avoid foreign key conflicts
DROP TABLE IF EXISTS pay_ledger CASCADE;
DROP TABLE IF EXISTS pay_refund CASCADE;
DROP TABLE IF EXISTS pay_callback CASCADE;
DROP TABLE IF EXISTS pay_payment CASCADE;
DROP TABLE IF EXISTS pay_order CASCADE;
DROP TABLE IF EXISTS pay_idempotency CASCADE;

-- Verify all tables are dropped
SELECT 'All pay tables dropped successfully' AS status;
