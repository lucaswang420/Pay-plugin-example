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
#include "PaymentService.h"
#include "RefundService.h"
#include "drogon_pay/PaymentChannel.h"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <trantor/net/EventLoop.h>

class ReconciliationService
{
  public:
    ReconciliationService(
      std::shared_ptr<PaymentService> paymentService,
      std::shared_ptr<RefundService> refundService,
      std::map<std::string, drogon_pay::PaymentChannelPtr> channels,
      std::shared_ptr<drogon::orm::DbClient> dbClient
    );

    // Timers run on `loop` when provided (PayPlugin passes its dedicated
    // worker loop); defaults to the app main loop for backward compatibility.
    void startReconcileTimer(trantor::EventLoop *loop = nullptr);
    void stopReconcileTimer();

    // Apply "reconcile" config block values (interval_seconds/batch_size).
    void setReconcileOptions(int intervalSeconds, int batchSize);

    void reconcile(std::function<void(int success, int failed)> &&callback);

  private:
    void syncPendingWeChatOrders(const std::shared_ptr<int> &failedCount);
    void syncPendingAlipayOrders(const std::shared_ptr<int> &failedCount);
    void syncPendingRefunds(const std::shared_ptr<int> &failedCount);

    bool isWeChatConfigured() const;
    bool isAlipayConfigured() const;

    drogon_pay::PaymentChannelPtr findChannel(const std::string &name) const;

    std::shared_ptr<PaymentService> paymentService_;
    std::shared_ptr<RefundService> refundService_;
    std::map<std::string, drogon_pay::PaymentChannelPtr> channels_;
    std::shared_ptr<drogon::orm::DbClient> dbClient_;
    trantor::EventLoop *timerLoop_{nullptr};
    trantor::TimerId reconcileTimerId_;
    int reconcileIntervalSeconds_;
    int reconcileBatchSize_;
};
