#include "ReconciliationService.h"
#include <drogon/drogon.h>

using namespace drogon;

ReconciliationService::ReconciliationService(
    std::shared_ptr<PaymentService> paymentService,
    std::shared_ptr<RefundService> refundService,
    std::shared_ptr<drogon::orm::DbClient> dbClient)
    : paymentService_(paymentService), refundService_(refundService),
      dbClient_(dbClient), reconcileTimerId_(0),
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

    // TODO: Implement full reconciliation logic
    syncPendingOrders();
    syncPendingRefunds();

    callback(0, 0);
}

void ReconciliationService::syncPendingOrders() {
    // TODO: Extract business logic from PayPlugin.cc
}

void ReconciliationService::syncPendingRefunds() {
    // TODO: Extract business logic from PayPlugin.cc
}
