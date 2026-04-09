-- Performance indexes for Pay Plugin

CREATE INDEX IF NOT EXISTS idx_pay_order_user_id ON pay_order(user_id);
CREATE INDEX IF NOT EXISTS idx_pay_order_status ON pay_order(status);
CREATE INDEX IF NOT EXISTS idx_pay_order_created_at ON pay_order(created_at);

CREATE INDEX IF NOT EXISTS idx_pay_payment_order_no ON pay_payment(order_no);
CREATE INDEX IF NOT EXISTS idx_pay_payment_status ON pay_payment(status);

CREATE INDEX IF NOT EXISTS idx_pay_refund_order_no ON pay_refund(order_no);
CREATE INDEX IF NOT EXISTS idx_pay_refund_status ON pay_refund(status);

CREATE INDEX IF NOT EXISTS idx_pay_callback_order_no ON pay_callback(order_no);
CREATE INDEX IF NOT EXISTS idx_pay_callback_processed ON pay_callback(processed);

CREATE INDEX IF NOT EXISTS idx_pay_idempotency_key ON pay_idempotency(key);
CREATE INDEX IF NOT EXISTS idx_pay_idempotency_created_at ON pay_idempotency(created_at);

CREATE INDEX IF NOT EXISTS idx_pay_ledger_user_id ON pay_ledger(user_id);
CREATE INDEX IF NOT EXISTS idx_pay_ledger_order_no ON pay_ledger(order_no);
CREATE INDEX IF NOT EXISTS idx_pay_ledger_entry_type ON pay_ledger(entry_type);
