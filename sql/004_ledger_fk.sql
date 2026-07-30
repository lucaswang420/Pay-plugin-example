-- Foreign keys for pay_ledger to enforce referential integrity at the DB layer
-- (P2 hardening). Previously the ledger had no FK constraints, so a buggy or
-- future code path could insert ledger rows pointing at non-existent orders or
-- payments, silently corrupting the accounting trail.
--
-- payment_no is nullable in pay_ledger (some entry types may not have one), so
-- the FK only applies when payment_no IS NOT NULL (standard SQL FK semantics).
--
-- Idempotent: uses a DO block so re-running does not error if the constraints
-- already exist.

DO $$
BEGIN
    IF NOT EXISTS (
        SELECT 1 FROM pg_constraint WHERE conname = 'pay_ledger_order_no_fkey'
    ) THEN
        ALTER TABLE pay_ledger
            ADD CONSTRAINT pay_ledger_order_no_fkey
            FOREIGN KEY (order_no) REFERENCES pay_order(order_no);
    END IF;

    IF NOT EXISTS (
        SELECT 1 FROM pg_constraint WHERE conname = 'pay_ledger_payment_no_fkey'
    ) THEN
        ALTER TABLE pay_ledger
            ADD CONSTRAINT pay_ledger_payment_no_fkey
            FOREIGN KEY (payment_no) REFERENCES pay_payment(payment_no);
    END IF;
END $$;
