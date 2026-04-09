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
    std::string paymentNo;
    std::string refundNo;
    std::string amount;
    std::string reason;
    std::string notifyUrl;
    std::string fundsAccount;
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
        const Json::Value& result,
        std::function<void(const std::string& status)>&& callback
    );

private:
    std::shared_ptr<WechatPayClient> wechatClient_;
    std::shared_ptr<drogon::orm::DbClient> dbClient_;
    std::shared_ptr<IdempotencyService> idempotencyService_;

    // Helper methods for refund processing
    void proceedRefund(
        const CreateRefundRequest& request,
        RefundCallback&& callback
    );

    void proceedOrderFlow(
        const CreateRefundRequest& request,
        const std::string& refundNo,
        const std::string& orderNo,
        const std::string& paymentNo,
        const std::string& amount,
        const std::string& reason,
        RefundCallback&& callback
    );

    void proceedWithAmountCheck(
        const CreateRefundRequest& request,
        const std::string& refundNo,
        const std::string& orderNo,
        const std::string& paymentNo,
        const std::string& amount,
        int64_t refundFen,
        int64_t totalFen,
        const std::string& currency,
        const std::string& reason,
        RefundCallback&& callback
    );

    void proceedWithInProgressCheck(
        const CreateRefundRequest& request,
        const std::string& refundNo,
        const std::string& orderNo,
        const std::string& paymentNo,
        const std::string& amount,
        int64_t refundFen,
        int64_t totalFen,
        const std::string& currency,
        const std::string& reason,
        RefundCallback&& callback
    );

    void proceedWithInsert(
        const CreateRefundRequest& request,
        const std::string& refundNo,
        const std::string& orderNo,
        const std::string& paymentNo,
        const std::string& amount,
        int64_t refundFen,
        int64_t totalFen,
        const std::string& currency,
        const std::string& reason,
        RefundCallback&& callback
    );

    void proceedWithRefundInsert(
        const CreateRefundRequest& request,
        const std::string& refundNo,
        const std::string& orderNo,
        const std::string& paymentNo,
        const std::string& amount,
        int64_t refundFen,
        int64_t totalFen,
        const std::string& currency,
        const std::string& reason,
        RefundCallback&& callback
    );

    void updateRefundWithError(
        const std::string& refundNo,
        const std::string& errorMessage,
        const Json::Value& errJson
    );

    void updateRefundWithSuccess(
        const std::string& refundNo,
        const std::string& refundStatus,
        const std::string& refundId,
        const Json::Value& result,
        const std::string& orderNo,
        const std::string& paymentNo,
        const std::string& amount,
        RefundCallback&& callback
    );
};
