#pragma once

#include <drogon/orm/DbClient.h>
#include "../plugins/WechatPayClient.h"
#include <json/json.h>
#include <functional>
#include <memory>
#include <string>

class CallbackService {
public:
    using CallbackResult = std::function<void(const Json::Value& result, const std::error_code& error)>;

    CallbackService(
        std::shared_ptr<WechatPayClient> wechatClient,
        std::shared_ptr<drogon::orm::DbClient> dbClient
    );

    void handlePaymentCallback(
        const std::string& body,
        const std::string& signature,
        const std::string& timestamp,
        const std::string& serialNo,
        CallbackResult&& callback
    );

    void handleRefundCallback(
        const std::string& body,
        const std::string& signature,
        const std::string& timestamp,
        const std::string& serialNo,
        CallbackResult&& callback
    );

private:
    bool verifySignature(
        const std::string& body,
        const std::string& signature,
        const std::string& timestamp,
        const std::string& serialNo
    );

    std::shared_ptr<WechatPayClient> wechatClient_;
    std::shared_ptr<drogon::orm::DbClient> dbClient_;
};
