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

void AlipaySandboxClient::sendRequest(const std::string &method,
                                     const Json::Value &bizContent,
                                     JsonCallback &&callback)
{
    LOG_ERROR << "sendRequest called for method: " << method;
    try {
        // Build common parameters
        Json::Value commonParams = buildCommonParams();
        commonParams["method"] = method;
        commonParams["biz_content"] = bizContent.toStyledString();
        commonParams["charset"] = "utf-8";
        commonParams["version"] = "1.0";
        commonParams["sign_type"] = "RSA2";

        // Build string to sign
        std::string signData;
        std::vector<std::string> keys;

        for (const auto &key : commonParams.getMemberNames()) {
            keys.push_back(key);
        }

        std::sort(keys.begin(), keys.end());

        for (const auto &key : keys) {
            if (!commonParams[key].isNull()) {
                if (!signData.empty()) {
                    signData += "&";
                }
                signData += key + "=" + commonParams[key].asString();
            }
        }

        // Sign the data
        std::string signature = sign(signData);
        commonParams["sign"] = signature;

        // Build request body (form format)
        std::string requestBody;
        for (const auto &key : keys) {
            if (!commonParams[key].isNull()) {
                if (!requestBody.empty()) {
                    requestBody += "&";
                }
                requestBody += key + "=" + drogon::utils::urlEncode(commonParams[key].asString());
            }
        }

        LOG_INFO << "Alipay request: " << method << ", URL: " << gatewayUrl_;

        // Use the complete gateway URL for creating HTTP client
        auto client = drogon::HttpClient::newHttpClient(gatewayUrl_);

        auto req = drogon::HttpRequest::newHttpRequest();
        req->setMethod(drogon::Post);
        req->setPath("/gateway.do");
        req->setContentTypeCode(drogon::CT_APPLICATION_X_FORM);
        req->setBody(requestBody);

        client->sendRequest(req, [this, callback, method](drogon::ReqResult result,
                                                        const drogon::HttpResponsePtr &response) {
            try {
                if (!response) {
                    LOG_ERROR << "Alipay HTTP error: No response";
                    Json::Value errorJson;
                    errorJson["error"] = "No response from Alipay server";
                    callback(Json::Value(), errorJson.toStyledString());
                    return;
                }

                // Check HTTP status code
                auto statusCode = response->getStatusCode();
                if (statusCode != drogon::k200OK) {
                    LOG_ERROR << "Alipay HTTP error: status " << statusCode;
                    Json::Value errorJson;
                    errorJson["error"] = "HTTP status code: " + std::to_string(statusCode);
                    callback(Json::Value(), errorJson.toStyledString());
                    return;
                }

                auto body = response->body();
                LOG_DEBUG << "Alipay response: " << body;

                // Parse JSON response
                Json::Value root;

                // Use CharReaderBuilder instead of deprecated Reader
                Json::CharReaderBuilder builder;
                std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
                std::string errors;
                std::string bodyStr = std::string(body);
                const char* str = bodyStr.c_str();

                if (!reader->parse(str, str + bodyStr.length(), &root, &errors)) {
                    LOG_ERROR << "Failed to parse Alipay response: " << body;
                    Json::Value error;
                    error["error"] = "Invalid response format";
                    error["raw_response"] = std::string(body);
                    callback(Json::Value(), error.toStyledString());
                    return;
                }

                // Check for error response
                if (root.isMember("error_response")) {
                    callback(Json::Value(), root["error_response"].toStyledString());
                    return;
                }

                // Extract the response
                std::string responseKey = "alipay_" + method + "_response";
                if (root.isMember(responseKey)) {
                    callback(root[responseKey], "");
                } else {
                    // Fallback: try to find any key with "response"
                    for (const auto &key : root.getMemberNames()) {
                        if (key.find("response") != std::string::npos) {
                            callback(root[key], "");
                            return;
                        }
                    }
                    callback(root, "");
                }
            } catch (const std::exception &e) {
                LOG_ERROR << "Alipay callback error: " << e.what();
                Json::Value error;
                error["error"] = e.what();
                callback(Json::Value(), error.asString());
            }
        });

    } catch (const std::exception &e) {
        LOG_ERROR << "AlipaySandboxClient::sendRequest error: " << e.what();
        Json::Value error;
        error["error"] = e.what();
        callback(Json::Value(), error.asString());
    }
}

std::string AlipaySandboxClient::sign(const std::string &data) const
{
    std::string privateKeyPem;

    // Read private key file
    std::ifstream file(privateKeyPath_);
    if (!file.is_open()) {
        LOG_ERROR << "Failed to open private key file: " << privateKeyPath_;
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    privateKeyPem = buffer.str();

    if (privateKeyPem.empty()) {
        LOG_ERROR << "Empty private key file";
        return "";
    }

    // Read private key using OpenSSL
    BIO* bio = BIO_new_mem_buf(privateKeyPem.c_str(), static_cast<int>(privateKeyPem.length()));
    if (!bio) {
        LOG_ERROR << "Failed to create BIO for private key";
        return "";
    }

    EVP_PKEY* pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    BIO_free_all(bio);

    if (!pkey) {
        LOG_ERROR << "Failed to read private key";
        return "";
    }

    // RSA sign using SHA256
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    EVP_DigestSignInit(mdctx, nullptr, EVP_sha256(), nullptr, pkey);

    EVP_DigestSignUpdate(mdctx, data.c_str(), data.length());

    size_t signatureLen = 0;
    EVP_DigestSignFinal(mdctx, nullptr, &signatureLen);

    std::vector<unsigned char> signature(signatureLen);
    EVP_DigestSignFinal(mdctx, signature.data(), &signatureLen);

    EVP_MD_CTX_free(mdctx);
    EVP_PKEY_free(pkey);

    // Convert to uppercase hex string
    std::stringstream ss;
    for (size_t i = 0; i < signatureLen; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(signature[i]);
    }

    return ss.str();
}

bool AlipaySandboxClient::verify(const std::string &data,
                                const std::string &signature) const
{
    std::string publicKeyPem;

    // Read Alipay public key file
    std::ifstream file(alipayPublicKeyPath_);
    if (!file.is_open()) {
        LOG_ERROR << "Failed to open Alipay public key file: " << alipayPublicKeyPath_;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    publicKeyPem = buffer.str();

    if (publicKeyPem.empty()) {
        LOG_ERROR << "Empty Alipay public key file";
        return false;
    }

    // Read public key
    BIO* bio = BIO_new_mem_buf(publicKeyPem.c_str(), static_cast<int>(publicKeyPem.length()));
    if (!bio) {
        LOG_ERROR << "Failed to create BIO for public key";
        return false;
    }

    EVP_PKEY* pkey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free_all(bio);

    if (!pkey) {
        LOG_ERROR << "Failed to read public key";
        return false;
    }

    // Convert hex signature to binary
    std::vector<unsigned char> binarySig;
    for (size_t i = 0; i < signature.length(); i += 2) {
        std::string byteStr = signature.substr(i, 2);
        unsigned char byte = static_cast<unsigned char>(std::stoi(byteStr, nullptr, 16));
        binarySig.push_back(byte);
    }

    // RSA verify
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    EVP_DigestVerifyInit(mdctx, nullptr, EVP_sha256(), nullptr, pkey);
    EVP_DigestVerifyUpdate(mdctx, data.c_str(), data.length());

    int result = EVP_DigestVerifyFinal(mdctx, binarySig.data(), binarySig.size());

    EVP_MD_CTX_free(mdctx);
    EVP_PKEY_free(pkey);

    return result == 1;
}

Json::Value AlipaySandboxClient::buildCommonParams() const
{
    Json::Value params;
    params["app_id"] = appId_;
    params["timestamp"] = std::to_string(std::time(nullptr));
    params["nonce"] = generateUUID();
    return params;
}
