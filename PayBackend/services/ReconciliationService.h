#pragma once

#include <drogon/orm/DbClient.h>
#include "PaymentService.h"
#include "RefundService.h"
#include <functional>
#include <memory>
#include <string>
#include <trantor/net/EventLoop.h>

class ReconciliationService {
public:
    ReconciliationService(
        std::shared_ptr<PaymentService> paymentService,
        std::shared_ptr<RefundService> refundService,
        std::shared_ptr<drogon::orm::DbClient> dbClient
    );

    void startReconcileTimer();
    void stopReconcileTimer();

    void reconcile(
        std::function<void(int success, int failed)>&& callback
    );

private:
    void syncPendingOrders();
    void syncPendingRefunds();

    std::shared_ptr<PaymentService> paymentService_;
    std::shared_ptr<RefundService> refundService_;
    std::shared_ptr<drogon::orm::DbClient> dbClient_;
    trantor::TimerId reconcileTimerId_;
    int reconcileIntervalSeconds_;
    int reconcileBatchSize_;
};
