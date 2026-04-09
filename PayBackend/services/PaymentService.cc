#include "PaymentService.h"
#include "../models/PayOrder.h"
#include "../models/PayPayment.h"
#include "../utils/PayUtils.h"
#include <drogon/drogon.h>
#include <random>
#include <sstream>

using namespace drogon;
using namespace drogon::orm;

PaymentService::PaymentService(
    std::shared_ptr<WechatPayClient> wechatClient,
    std::shared_ptr<DbClient> dbClient,
    nosql::RedisClientPtr redisClient,
    std::shared_ptr<IdempotencyService> idempotencyService)
    : wechatClient_(wechatClient), dbClient_(dbClient),
      redisClient_(redisClient), idempotencyService_(idempotencyService) {
}

void PaymentService::createPayment(
    const CreatePaymentRequest& request,
    const std::string& idempotencyKey,
    PaymentCallback&& callback) {

    // Calculate request hash for idempotency
    std::string requestStr = Json::writeString(Json::StreamWriterBuilder(),
        [&request]() {
            Json::Value req;
            req["order_no"] = request.orderNo;
            req["amount"] = request.amount;
            req["currency"] = request.currency;
            req["description"] = request.description;
            return req;
        }());

    // Simple hash (in production, use proper cryptographic hash)
    std::string requestHash = std::to_string(std::hash<std::string>{}(requestStr));

    // Check idempotency
    idempotencyService_->checkAndSet(
        idempotencyKey,
        requestHash,
        [&request]() {
            Json::Value req;
            req["order_no"] = request.orderNo;
            req["amount"] = request.amount;
            return req;
        }(),
        [this, request, idempotencyKey, callback](bool canProceed, const Json::Value& cachedResult) mutable {
            if (!canProceed) {
                // Idempotency conflict
                Json::Value error;
                error["code"] = 1004;
                error["message"] = "Idempotency conflict: different parameters for same key";
                callback(error, std::make_error_code(std::errc::operation_in_progress));
                return;
            }

            if (!cachedResult.isNull()) {
                // Return cached result
                callback(cachedResult, std::error_code());
                return;
            }

            // Proceed with payment creation
            std::string paymentNo = generatePaymentNo();
            int64_t totalFen = 0;
            if (!pay::utils::parseAmountToFen(request.amount, totalFen)) {
                Json::Value error;
                error["code"] = 1001;
                error["message"] = "Invalid amount format";
                callback(error, std::make_error_code(std::errc::invalid_argument));
                return;
            }

            proceedCreatePayment(request, paymentNo, totalFen, std::move(callback));
        }
    );
}

void PaymentService::proceedCreatePayment(
    const CreatePaymentRequest& request,
    const std::string& paymentNo,
    int64_t totalFen,
    PaymentCallback&& callback) {

    // TODO: Extract business logic from PayPlugin.cc
    // This is a placeholder - full implementation will be in subsequent tasks

    Json::Value response;
    response["code"] = 0;
    response["message"] = "Payment created (placeholder)";
    Json::Value data;
    data["payment_no"] = paymentNo;
    data["order_no"] = request.orderNo;
    response["data"] = data;

    callback(response, std::error_code());
}

void PaymentService::queryOrder(
    const std::string& orderNo,
    PaymentCallback&& callback) {

    // TODO: Implement in subsequent task
    Json::Value response;
    response["code"] = 0;
    response["message"] = "Query order (placeholder)";
    callback(response, std::error_code());
}

void PaymentService::syncOrderStatusFromWechat(
    const std::string& orderNo,
    std::function<void(const std::string& status)>&& callback) {

    // TODO: Implement in subsequent task
    callback("unknown");
}

void PaymentService::reconcileSummary(
    const std::string& date,
    PaymentCallback&& callback) {

    // TODO: Implement in subsequent task
    Json::Value response;
    response["code"] = 0;
    response["message"] = "Reconcile summary (placeholder)";
    callback(response, std::error_code());
}

std::string PaymentService::generatePaymentNo() {
    // Generate unique payment number
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 99999999);

    std::ostringstream oss;
    time_t now = std::time(nullptr);
    oss << "PAY" << std::put_time(std::localtime(&now), "%Y%m%d%H%M%S");
    oss << std::setfill('0') << std::setw(8) << dis(gen);

    return oss.str();
}
