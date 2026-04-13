#include "PayController.h"
#include "../services/PaymentService.h"
#include "../services/RefundService.h"
#include <drogon/orm/DbClient.h>
#include <json/json.h>
#include <stdexcept>

void PayController::createPayment(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    if (req->method() == Options)
    {
        auto resp = HttpResponse::newHttpResponse();
        callback(resp);
        return;
    }

    // Extract and validate JSON
    auto json = req->getJsonObject();
    if (!json)
    {
        Json::Value error;
        error["code"] = 400;
        error["message"] = "Invalid JSON";
        auto resp = HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    // Extract required fields
    if (!json->isMember("order_no") || !json->isMember("amount"))
    {
        Json::Value error;
        error["code"] = 400;
        error["message"] = "Missing required fields: order_no and amount";
        auto resp = HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    // Build request
    CreatePaymentRequest request;
    request.orderNo = (*json)["order_no"].asString();
    request.amount = (*json)["amount"].asString();
    request.currency = json->get("currency", "CNY").asString();
    request.description = json->get("description", "").asString();
    request.notifyUrl = json->get("notify_url", "").asString();

    // Get user_id from attributes (set by auth middleware)
    try
    {
        request.userId = req->attributes()->get<int64_t>("user_id");
    }
    catch (const std::exception&)
    {
        Json::Value error;
        error["code"] = 401;
        error["message"] = "Unauthorized: user_id not found";
        auto resp = HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    // Extract scene info if present
    if (json->isMember("scene_info"))
    {
        request.sceneInfo = (*json)["scene_info"];
    }

    // Get or generate idempotency key
    std::string idempotencyKey = req->getHeader("X-Idempotency-Key");
    if (idempotencyKey.empty())
    {
        idempotencyKey = req->getHeader("Idempotency-Key");
    }
    if (idempotencyKey.empty())
    {
        // Generate from request hash
        Json::Value requestJson;
        requestJson["order_no"] = request.orderNo;
        requestJson["amount"] = request.amount;
        requestJson["currency"] = request.currency;
        requestJson["description"] = request.description;
        requestJson["notify_url"] = request.notifyUrl;
        requestJson["user_id"] = static_cast<Json::Int64>(request.userId);
        requestJson["scene_info"] = request.sceneInfo;

        Json::StreamWriterBuilder builder;
        idempotencyKey = "payment:" + request.orderNo + ":" +
                        std::to_string(std::hash<std::string>{}(
                            Json::writeString(builder, requestJson)));
    }

    // Get service and call
    auto plugin = drogon::app().getPlugin<PayPlugin>();
    auto paymentService = plugin->paymentService();

    paymentService->createPayment(
        request, idempotencyKey,
        [callback](const Json::Value& result, const std::error_code& error) {
            auto resp = HttpResponse::newHttpJsonResponse(result);
            if (error)
            {
                resp->setStatusCode(k500InternalServerError);
            }
            callback(resp);
        }
    );
}

void PayController::queryOrder(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    if (req->method() == Options)
    {
        auto resp = HttpResponse::newHttpResponse();
        callback(resp);
        return;
    }

    // Get order_no from query parameter
    std::string orderNo = req->getParameter("order_no");
    if (orderNo.empty())
    {
        Json::Value error;
        error["code"] = 400;
        error["message"] = "Missing required parameter: order_no";
        auto resp = HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    // Get service and call
    auto plugin = drogon::app().getPlugin<PayPlugin>();
    auto paymentService = plugin->paymentService();

    paymentService->queryOrder(
        orderNo,
        [callback](const Json::Value& result, const std::error_code& error) {
            auto resp = HttpResponse::newHttpJsonResponse(result);
            if (error)
            {
                resp->setStatusCode(k500InternalServerError);
            }
            callback(resp);
        }
    );
}

void PayController::refund(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    if (req->method() == Options)
    {
        auto resp = HttpResponse::newHttpResponse();
        callback(resp);
        return;
    }

    // Extract and validate JSON
    auto json = req->getJsonObject();
    if (!json)
    {
        Json::Value error;
        error["code"] = 400;
        error["message"] = "Invalid JSON";
        auto resp = HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    // Extract required fields
    if (!json->isMember("order_no") || !json->isMember("amount"))
    {
        Json::Value error;
        error["code"] = 400;
        error["message"] = "Missing required fields: order_no and amount";
        auto resp = HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    // Build request
    CreateRefundRequest request;
    request.orderNo = (*json)["order_no"].asString();
    request.amount = (*json)["amount"].asString();
    request.reason = json->get("reason", "").asString();
    request.notifyUrl = json->get("notify_url", "").asString();
    request.fundsAccount = json->get("funds_account", "").asString();

    // Optional fields
    if (json->isMember("payment_no"))
    {
        request.paymentNo = (*json)["payment_no"].asString();
    }
    if (json->isMember("refund_no"))
    {
        request.refundNo = (*json)["refund_no"].asString();
    }

    // Get or generate idempotency key
    std::string idempotencyKey = req->getHeader("X-Idempotency-Key");
    if (idempotencyKey.empty())
    {
        idempotencyKey = req->getHeader("Idempotency-Key");
    }
    if (idempotencyKey.empty())
    {
        // Generate from request hash
        Json::Value requestJson;
        requestJson["order_no"] = request.orderNo;
        requestJson["payment_no"] = request.paymentNo;
        requestJson["amount"] = request.amount;
        requestJson["reason"] = request.reason;

        Json::StreamWriterBuilder builder;
        idempotencyKey = "refund:" + request.orderNo + ":" +
                        std::to_string(std::hash<std::string>{}(
                            Json::writeString(builder, requestJson)));
    }

    // Get service and call
    auto plugin = drogon::app().getPlugin<PayPlugin>();
    auto refundService = plugin->refundService();

    refundService->createRefund(
        request, idempotencyKey,
        [callback](const Json::Value& result, const std::error_code& error) {
            auto resp = HttpResponse::newHttpJsonResponse(result);
            if (error)
            {
                resp->setStatusCode(k500InternalServerError);
            }
            callback(resp);
        }
    );
}

void PayController::queryRefund(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    if (req->method() == Options)
    {
        auto resp = HttpResponse::newHttpResponse();
        callback(resp);
        return;
    }

    // Get refund_no from query parameter
    std::string refundNo = req->getParameter("refund_no");
    if (refundNo.empty())
    {
        Json::Value error;
        error["code"] = 400;
        error["message"] = "Missing required parameter: refund_no";
        auto resp = HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    // Get service and call
    auto plugin = drogon::app().getPlugin<PayPlugin>();
    auto refundService = plugin->refundService();

    refundService->queryRefund(
        refundNo,
        [callback](const Json::Value& result, const std::error_code& error) {
            auto resp = HttpResponse::newHttpJsonResponse(result);
            if (error)
            {
                // Map error code 1404 (Refund not found) to HTTP 404
                if (error.value() == 1404)
                {
                    resp->setStatusCode(k404NotFound);
                }
                else
                {
                    resp->setStatusCode(k500InternalServerError);
                }
            }
            callback(resp);
        }
    );
}

void PayController::reconcileSummary(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    if (req->method() == Options)
    {
        auto resp = HttpResponse::newHttpResponse();
        callback(resp);
        return;
    }

    // Get date from query parameter (default to today)
    std::string date = req->getParameter("date");
    if (date.empty())
    {
        // Use today's date in YYYY-MM-DD format
        auto now = trantor::Date::now();
        date = now.toCustomFormattedString("%Y-%m-%d", false);
    }

    // Get service and call
    auto plugin = drogon::app().getPlugin<PayPlugin>();
    auto paymentService = plugin->paymentService();

    paymentService->reconcileSummary(
        date,
        [callback](const Json::Value& result, const std::error_code& error) {
            auto resp = HttpResponse::newHttpJsonResponse(result);
            if (error)
            {
                resp->setStatusCode(k500InternalServerError);
            }
            callback(resp);
        }
    );
}
