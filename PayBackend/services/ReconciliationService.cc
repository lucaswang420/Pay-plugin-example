#include "ReconciliationService.h"
#include <drogon/drogon.h>

using namespace drogon;

ReconciliationService::ReconciliationService(
    std::shared_ptr<PaymentService> paymentService,
    std::shared_ptr<RefundService> refundService,
    std::shared_ptr<WechatPayClient> wechatClient,
    std::shared_ptr<AlipaySandboxClient> alipayClient,
    std::shared_ptr<drogon::orm::DbClient> dbClient)
    : paymentService_(paymentService), refundService_(refundService),
      wechatClient_(wechatClient), alipayClient_(alipayClient), dbClient_(dbClient), reconcileTimerId_(0),
      reconcileIntervalSeconds_(300), reconcileBatchSize_(50) {
}

void ReconciliationService::startReconcileTimer() {
    reconcileTimerId_ = drogon::app().getLoop()->runEvery(
        std::chrono::seconds(reconcileIntervalSeconds_),
        [this]() {
            this->reconcile([](int success, int failed) {
                LOG_INFO << "Reconciliation completed: success=" << success << ", failed=" << failed;
            });
        }
    );

    LOG_INFO << "Reconciliation timer started (interval: " << reconcileIntervalSeconds_ << "s)";
}

void ReconciliationService::stopReconcileTimer() {
    if (reconcileTimerId_) {
        drogon::app().getLoop()->invalidateTimer(reconcileTimerId_);
        reconcileTimerId_ = 0;
        LOG_INFO << "Reconciliation timer stopped";
    }
}

void ReconciliationService::reconcile(
    std::function<void(int success, int failed)>&& callback) {

    // Track success and failed counts
    auto successCount = std::make_shared<int>(0);
    auto failedCount = std::make_shared<int>(0);

    // Sync pending orders
    syncPendingOrders();

    // Sync pending refunds
    syncPendingRefunds();

    callback(*successCount, *failedCount);
}

void ReconciliationService::syncPendingOrders() {
    if (!dbClient_ || !wechatClient_) {
        return;
    }

    dbClient_->execSqlAsync(
        "SELECT order_no FROM pay_order WHERE status = $1 "
        "ORDER BY updated_at DESC LIMIT $2",
        [this](const drogon::orm::Result &r) {
            for (const auto &row : r) {
                const std::string orderNo =
                    row["order_no"].as<std::string>();
                wechatClient_->queryTransaction(
                    orderNo,
                    [this, orderNo](const Json::Value &result,
                                    const std::string &error) {
                        if (!error.empty()) {
                            LOG_WARN
                                << "Wechat query failed for order "
                                << orderNo << ": " << error;
                            return;
                        }
                        paymentService_->syncOrderStatusFromWechat(
                            orderNo,
                            result,
                            [](const std::string &) {});
                    });
            }
        },
        [](const drogon::orm::DrogonDbException &e) {
            LOG_ERROR << "Reconcile query error: "
                      << e.base().what();
        },
        "PAYING",
        reconcileBatchSize_);
}

void ReconciliationService::syncPendingRefunds() {
    if (!dbClient_ || !wechatClient_) {
        return;
    }

    dbClient_->execSqlAsync(
        "SELECT refund_no FROM pay_refund WHERE status IN ($1, $2) "
        "ORDER BY updated_at DESC LIMIT $3",
        [this](const drogon::orm::Result &r) {
            for (const auto &row : r) {
                const std::string refundNo =
                    row["refund_no"].as<std::string>();
                wechatClient_->queryRefund(
                    refundNo,
                    [this, refundNo](const Json::Value &result,
                                     const std::string &error) {
                        if (!error.empty()) {
                            LOG_WARN
                                << "Wechat refund query failed for "
                                << refundNo << ": " << error;
                            return;
                        }
                        refundService_->syncRefundStatusFromWechat(
                            refundNo,
                            result,
                            [](const std::string &) {});
                    });
            }
        },
        [](const drogon::orm::DrogonDbException &e) {
            LOG_ERROR << "Reconcile refund query error: "
                      << e.base().what();
        },
        "REFUND_INIT",
        "REFUNDING",
        reconcileBatchSize_);
}
