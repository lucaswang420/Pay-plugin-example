#include "AlipayCallbackController.h"
#include "../services/PaymentService.h"
#include <drogon/orm/DbClient.h>
#include <json/json.h>
#include <unordered_map>
#include <sstream>

using namespace drogon;

void AlipayCallbackController::notify(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    LOG_DEBUG << "[ALIPAY_CALLBACK] Received notification";

    // 支付宝回调使用表单格式，不是JSON
    std::string body = std::string(req->body());

    // 解析表单数据
    std::unordered_map<std::string, std::string> params;
    std::stringstream ss(body);
    std::string pair;

    while (std::getline(ss, pair, '&'))
    {
        size_t pos = pair.find('=');
        if (pos != std::string::npos)
        {
            std::string key = pair.substr(0, pos);
            std::string value = pair.substr(pos + 1);

            // URL decode using Drogon's urlDecode function
            std::string decoded = drogon::utils::urlDecode(value);
            params[key] = decoded;
        }
    }

    // Extract signature BEFORE removing it from params (verifyCallback also
    // excludes 'sign', but we need the value here).
    const std::string sign = params.count("sign") ? params["sign"] : std::string{};

    // Get the Alipay client. If it is not configured we MUST reject the callback
    // rather than processing it unverified — accepting an unverified callback
    // would let any party forge a payment-success notification (P0-1).
    auto plugin = drogon::app().getPlugin<PayPlugin>();
    auto alipayClient = plugin->alipayClient();
    if (!alipayClient)
    {
        LOG_ERROR << "[ALIPAY_CALLBACK] Alipay client not configured, rejecting callback";
        Json::Value response;
        response["code"] = "FAIL";
        response["message"] = "Alipay client not configured";
        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setContentTypeString("application/json");
        resp->addHeader("Content-Type", "application/json; charset=utf-8");
        callback(resp);
        return;
    }

    // Build the parameter set for signature verification. verifyCallback
    // expects a JSON object whose members (excluding 'sign'/'sign_type') are
    // sorted and joined as k=v to form the signed payload.
    Json::Value verifyParams;
    for (const auto &kv : params)
    {
        verifyParams[kv.first] = kv.second;
    }

    // Verify the callback signature BEFORE any database mutation. A failed
    // signature means the notification is not authentic and must not advance
    // any order state (P0-1: previously signature verification was never
    // invoked, allowing forged payment-success notifications).
    if (!alipayClient->verifyCallback(verifyParams, sign))
    {
        LOG_WARN << "[ALIPAY_CALLBACK] Signature verification failed, rejecting callback";
        Json::Value response;
        response["code"] = "FAIL";
        response["message"] = "signature verification failed";
        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setContentTypeString("application/json");
        resp->addHeader("Content-Type", "application/json; charset=utf-8");
        callback(resp);
        return;
    }
    LOG_INFO << "[ALIPAY_CALLBACK] Signature verified successfully";

    // 提取关键参数
    std::string outTradeNo = params["out_trade_no"];
    std::string tradeNo = params["trade_no"];
    std::string tradeStatus = params["trade_status"];
    std::string totalAmount = params["total_amount"];
    std::string appId = params["app_id"];
    std::string sellerId = params["seller_id"];
    std::string notifyTime = params["notify_time"];
    std::string notifyType = params["notify_type"];
    std::string notifyId = params["notify_id"];

    LOG_DEBUG << "[ALIPAY_CALLBACK] out_trade_no=" << outTradeNo << " trade_no=" << tradeNo
              << " trade_status=" << tradeStatus << " total_amount=" << totalAmount;

    // 构建JSON响应格式的result对象，供syncOrderStatusFromAlipay使用
    Json::Value alipayResult;
    alipayResult["code"] = "10000";  // 支付宝成功响应码
    alipayResult["msg"] = "Success";
    alipayResult["trade_no"] = tradeNo;
    alipayResult["out_trade_no"] = outTradeNo;
    alipayResult["trade_status"] = tradeStatus;
    alipayResult["total_amount"] = totalAmount;
    alipayResult["app_id"] = appId;
    alipayResult["seller_id"] = sellerId;
    alipayResult["notify_time"] = notifyTime;
    alipayResult["notify_type"] = notifyType;
    alipayResult["notify_id"] = notifyId;

    auto paymentService = plugin->paymentService();

    // 调用syncOrderStatusFromAlipay更新数据库
    paymentService->syncOrderStatusFromAlipay(
      outTradeNo, alipayResult, [callback, outTradeNo, tradeStatus](const std::string &status) {
          LOG_DEBUG << "[ALIPAY_CALLBACK] Sync completed for order " << outTradeNo
                    << " status=" << status;

          // 返回success响应给支付宝
          Json::Value response;
          response["code"] = "SUCCESS";
          response["message"] = "OK";

          LOG_INFO << "[AlipayCallback] Callback processed successfully: order_no=" << outTradeNo
                   << ", trade_status=" << tradeStatus << ", synced_status=" << status;

          auto resp = HttpResponse::newHttpJsonResponse(response);
          resp->setContentTypeString("application/json");
          resp->addHeader("Content-Type", "application/json; charset=utf-8");

          callback(resp);
      }
    );
}
