-- Pay Plugin Database Schema
-- Initial table creation

CREATE TABLE IF NOT EXISTS pay_order (
    id BIGSERIAL PRIMARY KEY,
    order_no VARCHAR(64) UNIQUE NOT NULL,
    user_id BIGINT NOT NULL,
    amount VARCHAR(32) NOT NULL,
    currency VARCHAR(8) NOT NULL DEFAULT 'CNY',
    status VARCHAR(32) NOT NULL DEFAULT 'pending',
    channel VARCHAR(32) NOT NULL DEFAULT 'alipay',
    title VARCHAR(512),
    expire_at TIMESTAMP,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS pay_payment (
    id BIGSERIAL PRIMARY KEY,
    payment_no VARCHAR(64) UNIQUE NOT NULL,
    order_no VARCHAR(64) NOT NULL REFERENCES pay_order(order_no),
    status VARCHAR(32) NOT NULL DEFAULT 'pending',
    amount VARCHAR(32) NOT NULL,
    request_payload TEXT,
    response_payload TEXT,
    channel_trade_no VARCHAR(64),
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS pay_refund (
    id BIGSERIAL PRIMARY KEY,
    refund_no VARCHAR(64) UNIQUE NOT NULL,
    order_no VARCHAR(64) NOT NULL REFERENCES pay_order(order_no),
    payment_no VARCHAR(64) NOT NULL REFERENCES pay_payment(payment_no),
    status VARCHAR(32) NOT NULL DEFAULT 'pending',
    amount VARCHAR(32) NOT NULL,
    channel_refund_no VARCHAR(64),
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS pay_callback (
    id BIGSERIAL PRIMARY KEY,
    payment_no VARCHAR(64) NOT NULL REFERENCES pay_payment(payment_no),
    raw_body TEXT NOT NULL,
    signature VARCHAR(512),
    serial_no VARCHAR(64),
    verified BOOLEAN NOT NULL DEFAULT FALSE,
    processed BOOLEAN NOT NULL DEFAULT FALSE,
    received_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS pay_idempotency (
    idempotency_key VARCHAR(128) PRIMARY KEY,
    request_hash VARCHAR(64) NOT NULL,
    response_snapshot TEXT,
    expire_at TIMESTAMP,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS pay_ledger (
    id BIGSERIAL PRIMARY KEY,
    user_id BIGINT NOT NULL,
    order_no VARCHAR(64) NOT NULL,
    payment_no VARCHAR(64),
    entry_type VARCHAR(32) NOT NULL,
    amount VARCHAR(32) NOT NULL,
    balance VARCHAR(32),
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
);
