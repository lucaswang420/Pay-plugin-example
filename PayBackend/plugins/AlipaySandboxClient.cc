#include "AlipaySandboxClient.h"
#include <drogon/HttpClient.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/utils/Utilities.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/bio.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <trantor/utils/Logger.h>

AlipaySandboxClient::AlipaySandboxClient(const Json::Value &config)
    : config_(config)
{
    appId_ = config.get("app_id", "").asString();
    sellerId_ = config.get("seller_id", "").asString();
    privateKeyPath_ = config.get("private_key_path", "").asString();
    alipayPublicKeyPath_ = config.get("alipay_public_key_path", "").asString();
    gatewayUrl_ = config.get("gateway_url", "https://openapi.alipaydev.com/gateway.do").asString();

    LOG_INFO << "AlipaySandboxClient initialized with AppID: " << appId_;
}

void AlipaySandboxClient::createTrade(const Json::Value &payload, JsonCallback &&callback)
{
    try {
        Json::Value bizContent = payload;
        if (!bizContent.isMember("out_trade_no")) bizContent["out_trade_no"] = generateUUID();
        if (!bizContent.isMember("total_amount")) bizContent["total_amount"] = "0.01";
        if (!bizContent.isMember("subject")) bizContent["subject"] = "测试订单";
        if (!bizContent.isMember("buyer_id")) bizContent["buyer_id"] = "2088102146225135";

        sendRequest("alipay.trade.create", bizContent, std::move(callback));
    } catch (const std::exception &e) {
        LOG_ERROR << "createTrade error: " << e.what();
        Json::Value error;
        error["error"] = e.what();
        callback(Json::Value(), error.asString());
    }
}

void AlipaySandboxClient::queryTrade(const std::string &outTradeNo, JsonCallback &&callback)
{
    try {
        Json::Value bizContent;
        bizContent["out_trade_no"] = outTradeNo;
        sendRequest("alipay.trade.query", bizContent, std::move(callback));
    } catch (const std::exception &e) {
        LOG_ERROR << "queryTrade error: " << e.what();
        Json::Value error;
        error["error"] = e.what();
        callback(Json::Value(), error.asString());
    }
}

void AlipaySandboxClient::refund(const Json::Value &payload, JsonCallback &&callback)
{
    try {
        Json::Value bizContent = payload;
        if (!bizContent.isMember("out_trade_no")) {
            Json::Value error;
            error["error"] = "out_trade_no is required";
            callback(Json::Value(), error.asString());
            return;
        }
        if (!bizContent.isMember("refund_amount")) bizContent["refund_amount"] = "0.01";
        sendRequest("alipay.trade.refund", bizContent, std::move(callback));
    } catch (const std::exception &e) {
        LOG_ERROR << "refund error: " << e.what();
        Json::Value error;
        error["error"] = e.what();
        callback(Json::Value(), error.asString());
    }
}

void AlipaySandboxClient::queryRefund(const std::string &outTradeNo, JsonCallback &&callback)
{
    try {
        Json::Value bizContent;
        bizContent["out_trade_no"] = outTradeNo;
        bizContent["query_type"] = "refund";
        sendRequest("alipay.trade.fastpay.refund.query", bizContent, std::move(callback));
    } catch (const std::exception &e) {
        LOG_ERROR << "queryRefund error: " << e.what();
        Json::Value error;
        error["error"] = e.what();
        callback(Json::Value(), error.asString());
    }
}

void AlipaySandboxClient::closeTrade(const std::string &outTradeNo, JsonCallback &&callback)
{
    try {
        Json::Value bizContent;
        bizContent["out_trade_no"] = outTradeNo;
        sendRequest("alipay.trade.close", bizContent, std::move(callback));
    } catch (const std::exception &e) {
        LOG_ERROR << "closeTrade error: " << e.what();
        Json::Value error;
        error["error"] = e.what();
        callback(Json::Value(), error.asString());
    }
}

bool AlipaySandboxClient::verifyCallback(const Json::Value &params, const std::string &signature)
{
    try {
        std::string data;
        std::vector<std::string> keys;
        for (const auto &key : params.getMemberNames()) {
            if (key != "sign" && key != "sign_type") keys.push_back(key);
        }
        std::sort(keys.begin(), keys.end());
        for (const auto &key : keys) {
            if (!params[key].isNull()) {
                if (!data.empty()) data += "&";
                data += key + "=" + params[key].asString();
            }
        }
        return verify(data, signature);
    } catch (const std::exception &e) {
        LOG_ERROR << "verifyCallback error: " << e.what();
        return false;
    }
}

std::string AlipaySandboxClient::generateUUID() const {
    return drogon::utils::getUuid();
}
