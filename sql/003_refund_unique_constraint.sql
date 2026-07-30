-- Defense-in-depth unique constraint for refund deduplication (P0-2 hardening).
--
-- The primary defense against refund over-issuance is the row-level lock
-- (SELECT ... FOR UPDATE on pay_payment) plus the atomic SUM-check + insert
-- inside a single transaction in RefundService::proceedWithInsert. This unique
-- constraint is a secondary, DB-enforced guard: it prevents two concurrent
-- transactions from inserting duplicate refund rows for the same payment even
-- if the application-level lock were ever bypassed or removed.
--
-- refund_no is already globally UNIQUE (see 001_init_pay_tables.sql), so this
-- composite constraint primarily codifies the (order_no, payment_no, refund_no)
-- relationship and gives the over-refund guard a stable index to rely on.

CREATE UNIQUE INDEX IF NOT EXISTS uq_pay_refund_order_payment_refund_no
    ON pay_refund(order_no, payment_no, refund_no);
