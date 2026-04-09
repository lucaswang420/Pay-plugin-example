#pragma once

#include <drogon/orm/DbClient.h>
#include <drogon/nosql/RedisClient.h>
#include "IdempotencyService.h"
#include "../plugins/WechatPayClient.h"
#include <json/json.h>
#include <functional>
#include <memory>
#include <string>

struct CreateRefundRequest {
    std::string orderNo;
    std::string refundNo;
    std::string amount;
    std::string reason;
    std::string notifyUrl;
};

class RefundService {
public:
    using RefundCallback = std::function<void(const Json::Value& result, const std::error_code& error)>;

    RefundService(
        std::shared_ptr<WechatPayClient> wechatClient,
        std::shared_ptr<drogon::orm::DbClient> dbClient,
        std::shared_ptr<IdempotencyService> idempotencyService
    );

    void createRefund(
        const CreateRefundRequest& request,
        const std::string& idempotencyKey,
        RefundCallback&& callback
    );

    void queryRefund(
        const std::string& refundNo,
        RefundCallback&& callback
    );

    void syncRefundStatusFromWechat(
        const std::string& refundNo,
        std::function<void(const std::string& status)>&& callback
    );

private:
    std::shared_ptr<WechatPayClient> wechatClient_;
    std::shared_ptr<drogon::orm::DbClient> dbClient_;
    std::shared_ptr<IdempotencyService> idempotencyService_;
};
