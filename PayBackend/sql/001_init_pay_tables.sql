-- Pay Plugin Database Schema
-- Initial table creation

CREATE TABLE IF NOT EXISTS pay_order (
    id BIGSERIAL PRIMARY KEY,
    order_no VARCHAR(64) UNIQUE NOT NULL,
    user_id BIGINT NOT NULL,
    amount NUMERIC(18,2) NOT NULL,
    currency VARCHAR(8) NOT NULL DEFAULT 'CNY',
    status VARCHAR(24) NOT NULL,
    channel VARCHAR(16) NOT NULL,
    title VARCHAR(128) NOT NULL,
    expire_at TIMESTAMP,
    created_at TIMESTAMP NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS pay_payment (
    id BIGSERIAL PRIMARY KEY,
    payment_no VARCHAR(64) UNIQUE NOT NULL,
    order_no VARCHAR(64) NOT NULL REFERENCES pay_order(order_no),
    channel_trade_no VARCHAR(64),
    status VARCHAR(24) NOT NULL,
    amount NUMERIC(18,2) NOT NULL,
    request_payload TEXT,
    response_payload TEXT,
    created_at TIMESTAMP NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS pay_refund (
    id BIGSERIAL PRIMARY KEY,
    refund_no VARCHAR(64) UNIQUE NOT NULL,
    order_no VARCHAR(64) NOT NULL REFERENCES pay_order(order_no),
    payment_no VARCHAR(64) NOT NULL REFERENCES pay_payment(payment_no),
    channel_refund_no VARCHAR(64),
    status VARCHAR(24) NOT NULL,
    amount NUMERIC(18,2) NOT NULL,
    response_payload TEXT,
    created_at TIMESTAMP NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS pay_callback (
    id BIGSERIAL PRIMARY KEY,
    payment_no VARCHAR(64) NOT NULL,
    raw_body TEXT NOT NULL,
    signature VARCHAR(512),
    serial_no VARCHAR(64),
    verified BOOLEAN NOT NULL DEFAULT FALSE,
    processed BOOLEAN NOT NULL DEFAULT FALSE,
    received_at TIMESTAMP NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS pay_idempotency (
    idempotency_key VARCHAR(64) PRIMARY KEY,
    request_hash VARCHAR(64) NOT NULL,
    response_snapshot TEXT,
    expires_at TIMESTAMP
);

CREATE TABLE IF NOT EXISTS pay_ledger (
    id BIGSERIAL PRIMARY KEY,
    user_id BIGINT NOT NULL,
    order_no VARCHAR(64) NOT NULL,
    payment_no VARCHAR(64),
    entry_type VARCHAR(24) NOT NULL,
    amount NUMERIC(18,2) NOT NULL,
    balance NUMERIC(18,2),
    created_at TIMESTAMP NOT NULL DEFAULT NOW()
);
