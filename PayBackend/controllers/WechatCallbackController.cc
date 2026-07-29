#include "WechatCallbackController.h"
#include "../services/CallbackService.h"

void WechatCallbackController::notify(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    // Extract callback parameters from request
    std::string body = std::string(req->body());
    std::string signature = std::string(req->getHeader("Wechatpay-Signature"));
    std::string timestamp = std::string(req->getHeader("Wechatpay-Timestamp"));
    std::string nonce = std::string(req->getHeader("Wechatpay-Nonce"));
    std::string serialNo = std::string(req->getHeader("Wechatpay-Serial"));

    // Get CallbackService from Plugin
    auto plugin = drogon::app().getPlugin<PayPlugin>();
    auto callbackService = plugin->callbackService();

    // Route to appropriate callback handler based on event_type
    // Parse body to determine callback type
    Json::Value bodyJson;
    std::string eventType;

    // Use CharReaderBuilder instead of deprecated Reader
    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    std::string errors;
    const char *str = body.c_str();
    bool parseOk =
      reader->parse(str, str + body.length(), &bodyJson, &errors);

    // Validate JSON and event_type before routing (C2-1 fix).
    if (!parseOk)
    {
        LOG_WARN << "[WECHAT_CALLBACK] Invalid JSON body: " << errors;
        Json::Value response;
        response["code"] = 40002;
        response["message"] = "Invalid JSON body";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    if (!bodyJson.isMember("event_type"))
    {
        LOG_WARN << "[WECHAT_CALLBACK] Missing event_type in callback body";
        Json::Value response;
        response["code"] = 40003;
        response["message"] = "Missing event_type";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    eventType = bodyJson["event_type"].asString();

    // Reject unknown event types that are neither TRANSACTION nor REFUND.
    if (
      eventType != "TRANSACTION.SUCCESS" && eventType.find("REFUND") == std::string::npos
    )
    {
        LOG_WARN << "[WECHAT_CALLBACK] Unknown event_type: " << eventType;
        Json::Value response;
        response["code"] = 40004;
        response["message"] = "Unknown event_type: " + eventType;
        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    // Route to payment or refund callback handler
    if (eventType.find("REFUND") != std::string::npos)
    {
        // Handle refund callback
        callbackService->handleRefundCallback(
          body,
          signature,
          timestamp,
          nonce,
          serialNo,
          [callback](const Json::Value &result, const std::error_code &error) {
              auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
              if (error)
              {
                  resp->setStatusCode(drogon::k500InternalServerError);
              }
              callback(resp);
          }
        );
    }
    else
    {
        // Handle payment callback (default)
        callbackService->handlePaymentCallback(
          body,
          signature,
          timestamp,
          nonce,
          serialNo,
          [callback](const Json::Value &result, const std::error_code &error) {
              auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
              if (error)
              {
                  resp->setStatusCode(drogon::k500InternalServerError);
              }
              callback(resp);
          }
        );
    }
}
