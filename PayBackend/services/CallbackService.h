#pragma once

// ============================================================================
// TECHNICAL DEBT NOTICE
// ============================================================================
// This service was NOT implemented using Test-Driven Development (TDD).
//
// Created: 2026-04-11
// TDD Status: Non-compliant
// Test Coverage: Integration tests only (no unit tests)
//
// Issues:
// - Implemented without failing tests first
// - Unit tests were removed due to being incomplete (nullptr mocks)
// - Behavior verified through integration tests only
//
// Priority: Medium
// Planned Action: Reimplement with TDD when time permits
//
// Integration Test Status: See test/ directory for integration tests
// that verify this service's behavior.
//
// Note: When modifying this service, consider adding TDD-compliant
// unit tests for new functionality.
// ============================================================================

#include <drogon/orm/DbClient.h>
#include <drogon/nosql/RedisClient.h>
#include "../plugins/WechatPayClient.h"
#include <json/json.h>
#include <functional>
#include <memory>
#include <string>

class CallbackService
{
  public:
    using CallbackResult =
      std::function<void(const Json::Value &result, const std::error_code &error)>;

    // `redisClient` is used for the callback nonce cache (replay protection,
    // P1-1). May be null: when absent the nonce check is skipped (fail-open;
    // the DB idempotency table still dedupes processing).
    CallbackService(
      std::shared_ptr<WechatPayClient> wechatClient,
      std::shared_ptr<drogon::orm::DbClient> dbClient,
      drogon::nosql::RedisClientPtr redisClient = nullptr
    );

    void handlePaymentCallback(
      const std::string &body,
      const std::string &signature,
      const std::string &timestamp,
      const std::string &nonce,
      const std::string &serialNo,
      CallbackResult &&callback
    );

    void handleRefundCallback(
      const std::string &body,
      const std::string &signature,
      const std::string &timestamp,
      const std::string &nonce,
      const std::string &serialNo,
      CallbackResult &&callback
    );

  private:
    bool verifySignature(
      const std::string &body,
      const std::string &signature,
      const std::string &timestamp,
      const std::string &nonce,
      const std::string &serialNo
    );

    // Replay protection (P1-1): returns false and sets errorMsg when the
    // callback timestamp is outside the ±300s freshness window or unparseable.
    bool isTimestampFresh(const std::string &timestamp, std::string &errorMsg);

    // Replay protection (P1-1): atomically reserve the callback nonce in Redis
    // (SET NX EX 360). Calls `proceed(true)` on first sight, `proceed(false)`
    // if the nonce was already seen (replay). Fail-open: if redisClient_ is
    // null or Redis errors, logs a warning and calls `proceed(true)` so a Redis
    // outage does not drop legitimate callbacks (the DB idempotency table
    // remains the source of truth for duplicate processing).
    void checkNonce(
      const std::string &nonce,
      std::function<void(bool firstSight)> proceed
    );

    std::shared_ptr<WechatPayClient> wechatClient_;
    std::shared_ptr<drogon::orm::DbClient> dbClient_;
    drogon::nosql::RedisClientPtr redisClient_;
};
