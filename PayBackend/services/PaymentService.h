#pragma once

#include <drogon/orm/DbClient.h>
#include <drogon/nosql/RedisClient.h>
#include "IdempotencyService.h"
#include "../plugins/WechatPayClient.h"
#include <json/json.h>
#include <functional>
#include <memory>
#include <string>

struct CreatePaymentRequest {
    std::string orderNo;
    std::string amount;
    std::string currency;
    std::string description;
    std::string notifyUrl;
    int64_t userId;
    Json::Value sceneInfo;
};

class PaymentService {
public:
    using PaymentCallback = std::function<void(const Json::Value& result, const std::error_code& error)>;

    PaymentService(
        std::shared_ptr<WechatPayClient> wechatClient,
        std::shared_ptr<drogon::orm::DbClient> dbClient,
        drogon::nosql::RedisClientPtr redisClient,
        std::shared_ptr<IdempotencyService> idempotencyService
    );

    void createPayment(
        const CreatePaymentRequest& request,
        const std::string& idempotencyKey,
        PaymentCallback&& callback
    );

    void queryOrder(
        const std::string& orderNo,
        PaymentCallback&& callback
    );

    void syncOrderStatusFromWechat(
        const std::string& orderNo,
        const Json::Value& wechatResult,
        std::function<void(const std::string& status)>&& callback
    );

    void reconcileSummary(
        const std::string& date,
        PaymentCallback&& callback
    );

private:
    std::shared_ptr<WechatPayClient> wechatClient_;
    std::shared_ptr<drogon::orm::DbClient> dbClient_;
    drogon::nosql::RedisClientPtr redisClient_;
    std::shared_ptr<IdempotencyService> idempotencyService_;

    std::string generatePaymentNo();
    void proceedCreatePayment(
        const CreatePaymentRequest& request,
        const std::string& paymentNo,
        int64_t totalFen,
        PaymentCallback&& callback
    );
};
