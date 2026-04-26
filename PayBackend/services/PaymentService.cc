#include "PaymentService.h"
#include "../models/PayOrder.h"
#include "../models/PayPayment.h"
#include "../models/PayLedger.h"
#include "../models/PayIdempotency.h"
#include "../utils/PayUtils.h"
#include <drogon/drogon.h>
#include <random>
#include <sstream>
#include <iomanip>

using namespace drogon;
using namespace drogon::orm;

// Model type aliases for convenience
namespace {
    using PayOrderModel = drogon_model::pay_test::PayOrder;
    using PayPaymentModel = drogon_model::pay_test::PayPayment;
    using PayLedgerModel = drogon_model::pay_test::PayLedger;
    using PayIdempotencyModel = drogon_model::pay_test::PayIdempotency;
}

namespace {
    // Helper functions adapted from PayPlugin.cc
    void insertLedgerEntry(
        const std::shared_ptr<DbClient> &dbClient,
        int64_t userId,
        const std::string &orderNo,
        const std::string &paymentNo,
        const std::string &entryType,
        const std::string &amount) {
        if (!dbClient) {
            return;
        }

        auto insertRow = [dbClient, userId, orderNo, paymentNo, entryType, amount]() {
            PayLedgerModel ledger;
            ledger.setUserId(userId);
            ledger.setOrderNo(orderNo);
            if (paymentNo.empty()) {
                ledger.setPaymentNoToNull();
            } else {
                ledger.setPaymentNo(paymentNo);
            }
            ledger.setEntryType(entryType);
            ledger.setAmount(amount);
            ledger.setCreatedAt(trantor::Date::now());

            Mapper<PayLedgerModel> ledgerMapper(dbClient);
            ledgerMapper.insert(
                ledger,
                [](const PayLedgerModel &) {},
                [](const DrogonDbException &e) {
                    LOG_ERROR << "Ledger insert error: " << e.base().what();
                });
        };

        if (orderNo.empty() || entryType.empty()) {
            insertRow();
            return;
        }

        if (paymentNo.empty()) {
            dbClient->execSqlAsync(
                "SELECT 1 FROM pay_ledger WHERE order_no = $1 "
                "AND entry_type = $2 AND payment_no IS NULL LIMIT 1",
                [insertRow](const Result &rows) {
                    if (rows.empty()) {
                        insertRow();
                    }
                },
                [](const DrogonDbException &e) {
                    LOG_ERROR << "Ledger lookup error: " << e.base().what();
                },
                orderNo, entryType);
            return;
        }

        dbClient->execSqlAsync(
            "SELECT 1 FROM pay_ledger WHERE order_no = $1 "
            "AND entry_type = $2 AND payment_no = $3 LIMIT 1",
            [insertRow](const Result &rows) {
                if (rows.empty()) {
                    insertRow();
                }
            },
            [](const DrogonDbException &e) {
                LOG_ERROR << "Ledger lookup error: " << e.base().what();
            },
            orderNo, entryType, paymentNo);
    }

    void storeIdempotencySnapshot(
        const std::shared_ptr<DbClient> &dbClient,
        const std::string &idempotencyKey,
        const std::string &requestHash,
        const std::string &responseSnapshot,
        int64_t ttlSeconds) {
        if (!dbClient || idempotencyKey.empty()) {
            return;
        }

        PayIdempotencyModel idemp;
        idemp.setIdempotencyKey(idempotencyKey);
        idemp.setRequestHash(requestHash);
        idemp.setResponseSnapshot(responseSnapshot);
        const auto now = trantor::Date::now();
        const auto expiresAt = trantor::Date(
            now.microSecondsSinceEpoch() + ttlSeconds * static_cast<int64_t>(1000000));
        idemp.setExpireAt(expiresAt);

        Mapper<PayIdempotencyModel> idempMapper(dbClient);
        idempMapper.insert(
            idemp,
            [](const PayIdempotencyModel &) {},
            [](const DrogonDbException &e) {
                LOG_ERROR << "Idempotency insert error: " << e.base().what();
            });
    }

    std::string toRfc3339Utc(const trantor::Date &when) {
        const auto seconds = static_cast<time_t>(when.microSecondsSinceEpoch() / 1000000);
        std::tm tmUtc{};
        gmtime_s(&tmUtc, &seconds);
        char buffer[32]{};
        if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tmUtc) == 0) {
            return {};
        }
        return buffer;
    }
}

PaymentService::PaymentService(
    std::shared_ptr<WechatPayClient> wechatClient,
    std::shared_ptr<AlipaySandboxClient> alipayClient,
    std::shared_ptr<DbClient> dbClient,
    nosql::RedisClientPtr redisClient,
    std::shared_ptr<IdempotencyService> idempotencyService)
    : wechatClient_(wechatClient), alipayClient_(alipayClient), dbClient_(dbClient),
      redisClient_(redisClient), idempotencyService_(idempotencyService) {
}

void PaymentService::createPayment(
    const CreatePaymentRequest& request,
    const std::string& idempotencyKey,
    PaymentCallback&& callback) {

    // For Alipay QR code payment, directly use precreate API
    if (request.channel == "alipay") {
        createQRPayment(
            [&request]() {
                Json::Value req;
                req["order_no"] = request.orderNo;
                req["amount"] = request.amount;
                req["channel"] = request.channel;
                req["user_id"] = request.userId;
                req["subject"] = request.description.empty() ? "Payment" : request.description;
                return req;
            }(),
            std::move(callback)
        );
        return;
    }

    // Original createTrade logic for WeChat or other channels
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
                callback(error, std::error_code());
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
                callback(error, std::error_code());
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

    // Wrap callback in shared_ptr to prevent it from being destroyed during async operations
    auto sharedCb = std::make_shared<PaymentCallback>(std::move(callback));

    // Create order record in database
    Mapper<PayOrderModel> orderMapper(dbClient_);
    PayOrderModel order;
    order.setOrderNo(request.orderNo);
    order.setUserId(request.userId);
    order.setAmount(request.amount);
    order.setCurrency(request.currency);
    order.setStatus("CREATED");
    order.setChannel(request.channel);
    order.setTitle(request.description);
    order.setCreatedAt(trantor::Date::now());
    order.setUpdatedAt(trantor::Date::now());

    // Build payment request payload based on channel
    Json::Value payload;

    if (request.channel == "alipay") {
        // Alipay API format
        // Convert fen to yuan for Alipay (string format)
        std::ostringstream yuanStream;
        yuanStream << std::fixed << std::setprecision(2) << (totalFen / 100.0);
        const std::string totalAmountYuan = yuanStream.str();

        payload["total_amount"] = totalAmountYuan;
        payload["subject"] = request.description;  // Alipay uses 'subject' instead of 'description'
        payload["out_trade_no"] = request.orderNo;

        // Add buyer_id for sandbox testing
        const char* buyerIdEnv = std::getenv("ALIPAY_SANDBOX_BUYER_ID");
        if (buyerIdEnv && strlen(buyerIdEnv) > 0) {
            payload["buyer_id"] = std::string(buyerIdEnv);
        }

        if (!request.notifyUrl.empty()) {
            payload["notify_url"] = request.notifyUrl;
        }
    } else {
        // WeChat Pay API format (original format)
        payload["description"] = request.description;
        payload["out_trade_no"] = request.orderNo;
        payload["amount"]["total"] = static_cast<Json::Int64>(totalFen);
        payload["amount"]["currency"] = request.currency;

        if (!request.notifyUrl.empty()) {
            payload["notify_url"] = request.notifyUrl;
        }

        if (!request.sceneInfo.isNull()) {
            payload["scene_info"] = request.sceneInfo;
        }
    }

    const std::string requestPayload = pay::utils::toJsonString(payload);

    // Insert order into database
    try {
        orderMapper.insert(
            order,
            [this, request, paymentNo, payload, requestPayload, sharedCb](const PayOrderModel &) {
                // Create payment record
                Mapper<PayPaymentModel> paymentMapper(dbClient_);
                PayPaymentModel payment;
                payment.setOrderNo(request.orderNo);
                payment.setPaymentNo(paymentNo);
                payment.setStatus("INIT");
                payment.setAmount(request.amount);
            payment.setRequestPayload(requestPayload);
            payment.setCreatedAt(trantor::Date::now());
            payment.setUpdatedAt(trantor::Date::now());

            paymentMapper.insert(
                payment,
                [this, request, paymentNo, payload, sharedCb](const PayPaymentModel &) {
                    // Helper lambda to handle payment client response
                    auto paymentCallback = [this, request, paymentNo, sharedCb](
                        const Json::Value &result, const std::string &error) {

                        if (!error.empty()) {
                            // Handle payment error
                            Json::Value errJson;
                            errJson["error"] = error;
                            const std::string errPayload = pay::utils::toJsonString(errJson);

                            // Update payment status to FAILED
                            Mapper<PayPaymentModel> paymentMapper(dbClient_);
                            auto payCriteria = Criteria(
                                PayPaymentModel::Cols::_payment_no,
                                CompareOperator::EQ,
                                paymentNo);
                            paymentMapper.findOne(
                                payCriteria,
                                [this, errPayload, request, sharedCb](PayPaymentModel payment) {
                                    payment.setStatus("FAIL");
                                    payment.setResponsePayload(errPayload);
                                    payment.setUpdatedAt(trantor::Date::now());
                                    Mapper<PayPaymentModel> paymentUpdater(dbClient_);
                                    paymentUpdater.update(
                                        payment,
                                        [this, request, sharedCb](const size_t) {
                                            // Update order status to FAILED
                                            Mapper<PayOrderModel> orderMapper(dbClient_);
                                            auto orderCriteria = Criteria(
                                                PayOrderModel::Cols::_order_no,
                                                CompareOperator::EQ,
                                                request.orderNo);
                                            orderMapper.findOne(
                                                orderCriteria,
                                                [this, sharedCb](PayOrderModel order) {
                                                    order.setStatus("FAILED");
                                                    order.setUpdatedAt(trantor::Date::now());
                                                    Mapper<PayOrderModel> orderUpdater(dbClient_);
                                                    orderUpdater.update(
                                                        order,
                                                        [](const size_t) {},
                                                        [](const DrogonDbException &) {});
                                                },
                                                [sharedCb](const DrogonDbException &) {
                                                    if (*sharedCb) {
                                                        Json::Value response;
                                                        response["code"] = 1003;
                                                        response["message"] = "Database error during payment failure update";
                                                        (*sharedCb)(response, std::error_code());
                                                    }
                                                });
                                        },
                                        [sharedCb](const DrogonDbException &) {
                                            if (*sharedCb) {
                                                Json::Value response;
                                                response["code"] = 1003;
                                                response["message"] = "Database error during payment failure update";
                                                (*sharedCb)(response, std::error_code());
                                            }
                                        });
                                },
                                [sharedCb](const DrogonDbException &) {
                                    if (*sharedCb) {
                                        Json::Value response;
                                        response["code"] = 1003;
                                        response["message"] = "Database error during payment failure update";
                                        (*sharedCb)(response, std::error_code());
                                    }
                                });

                            // Return error response
                            if (*sharedCb) {
                                Json::Value response;
                                response["code"] = 1002;
                                std::string channelName = request.channel == "alipay" ? "Alipay" : "WeChat Pay";
                                response["message"] = channelName + " error: " + error;
                                (*sharedCb)(response, std::error_code());
                            }
                            return;
                        }

                        // Success - update payment and order status
                        const std::string responsePayload = pay::utils::toJsonString(result);

                        Mapper<PayPaymentModel> paymentMapper(dbClient_);
                        auto payCriteria = Criteria(
                            PayPaymentModel::Cols::_payment_no,
                            CompareOperator::EQ,
                            paymentNo);
                        paymentMapper.findOne(
                            payCriteria,
                            [this, request, paymentNo, result, responsePayload, sharedCb](
                                PayPaymentModel payment) {
                                payment.setStatus("PROCESSING");
                                payment.setResponsePayload(responsePayload);
                                payment.setUpdatedAt(trantor::Date::now());
                                Mapper<PayPaymentModel> paymentUpdater(dbClient_);
                                paymentUpdater.update(
                                    payment,
                                    [this, request, paymentNo, result, sharedCb](
                                        const size_t) {
                                        // Update order status to PAYING
                                        Mapper<PayOrderModel> orderMapper(dbClient_);
                                        auto orderCriteria = Criteria(
                                            PayOrderModel::Cols::_order_no,
                                            CompareOperator::EQ,
                                            request.orderNo);
                                        orderMapper.findOne(
                                            orderCriteria,
                                            [this, request, paymentNo, result, sharedCb](
                                                PayOrderModel order) {
                                                order.setStatus("PAYING");
                                                order.setUpdatedAt(trantor::Date::now());
                                                Mapper<PayOrderModel> orderUpdater(dbClient_);
                                                orderUpdater.update(
                                                    order,
                                                    [this, request, paymentNo, result, sharedCb](
                                                        const size_t) {
                                                        // Build success response
                                                        Json::Value response;
                                                        response["code"] = 0;
                                                        response["message"] = "Payment created successfully";
                                                        Json::Value data;
                                                        data["order_no"] = request.orderNo;
                                                        data["payment_no"] = paymentNo;
                                                        data["status"] = "PAYING";

                                                        // Add payment channel response details
                                                        if (request.channel == "alipay") {
                                                            // Alipay response
                                                            data["alipay_response"] = result;
                                                            const auto qrCode = result.get("qr_code", "").asString();
                                                            if (!qrCode.empty()) {
                                                                data["qr_code"] = qrCode;
                                                            }
                                                        } else {
                                                            // WeChat Pay response
                                                            data["wechat_response"] = result;
                                                            const auto codeUrl = result.get("code_url", "").asString();
                                                            if (!codeUrl.empty()) {
                                                                data["code_url"] = codeUrl;
                                                            }
                                                            const auto prepayId = result.get("prepay_id", "").asString();
                                                            if (!prepayId.empty()) {
                                                                data["prepay_id"] = prepayId;
                                                            }
                                                        }

                                                        response["data"] = data;
                                                        if (*sharedCb) {
                                                            (*sharedCb)(response, std::error_code());
                                                        }
                                                    },
                                                    [sharedCb](const DrogonDbException &e) {
                                                        if (*sharedCb) {
                                                            Json::Value response;
                                                            response["code"] = 1003;
                                                            response["message"] = "Database error: " + std::string(e.base().what());
                                                            (*sharedCb)(response, std::error_code());
                                                        }
                                                    });
                                            },
                                            [sharedCb](const DrogonDbException &e) {
                                                if (*sharedCb) {
                                                    Json::Value response;
                                                    response["code"] = 1003;
                                                    response["message"] = "Database error: " + std::string(e.base().what());
                                                    (*sharedCb)(response, std::error_code());
                                                }
                                            });
                                    },
                                    [sharedCb](const DrogonDbException &e) {
                                        if (*sharedCb) {
                                            Json::Value response;
                                            response["code"] = 1003;
                                            response["message"] = "Database error: " + std::string(e.base().what());
                                            (*sharedCb)(response, std::error_code());
                                        }
                                    });
                            },
                            [sharedCb](const DrogonDbException &e) {
                                if (*sharedCb) {
                                    Json::Value response;
                                    response["code"] = 1003;
                                    response["message"] = "Database error: " + std::string(e.base().what());
                                    (*sharedCb)(response, std::error_code());
                                }
                            });
                    };

                    // Route to appropriate payment client based on channel
                    LOG_DEBUG << "About to call payment client, channel: " << request.channel;
                    if (request.channel == "alipay") {
                        // Call Alipay Sandbox API to create trade
                        LOG_DEBUG << "Calling Alipay createTrade with payload: " << payload.toStyledString();
                        alipayClient_->createTrade(payload, paymentCallback);
                    } else {
                        // Call WeChat Pay API to create transaction
                        LOG_DEBUG << "Calling WeChat createTransactionNative";
                        wechatClient_->createTransactionNative(payload, paymentCallback);
                    }
                },
                [sharedCb](const DrogonDbException &e) {
                    if (*sharedCb) {
                        Json::Value response;
                        response["code"] = 1003;
                        response["message"] = "Database error: " + std::string(e.base().what());
                        (*sharedCb)(response, std::make_error_code(std::errc::io_error));
                    }
                });
        },
        [sharedCb](const DrogonDbException &e) {
            if (*sharedCb) {
                Json::Value response;
                response["code"] = 1003;
                response["message"] = "Database error: " + std::string(e.base().what());
                (*sharedCb)(response, std::make_error_code(std::errc::io_error));
            }
        });
    } catch (const std::exception& e) {
        if (*sharedCb) {
            Json::Value response;
            response["code"] = 1003;
            response["message"] = "Exception during payment creation: " + std::string(e.what());
            (*sharedCb)(response, std::make_error_code(std::errc::io_error));
        }
    }
}

void PaymentService::createQRPayment(
    const Json::Value& request,
    PaymentCallback&& callback) {

    // Wrap callback in shared_ptr to prevent it from being destroyed during async operations
    auto sharedCb = std::make_shared<PaymentCallback>(std::move(callback));

    // Extract parameters
    std::string orderNo = request.get("order_no", "").asString();
    std::string amount = request.get("amount", "").asString();
    std::string channel = request.get("channel", "alipay").asString();
    std::string subject = request.get("subject", "Payment").asString();

    if (orderNo.empty() || amount.empty()) {
        Json::Value response;
        response["code"] = 400;
        response["message"] = "Missing required parameters: order_no, amount";
        (*sharedCb)(response, std::make_error_code(std::errc::invalid_argument));
        return;
    }

    // Build QR payment payload for Alipay
    Json::Value payload;
    payload["out_trade_no"] = orderNo;
    payload["total_amount"] = amount;
    payload["subject"] = subject;

    if (request.isMember("buyer_id")) {
        payload["buyer_id"] = request["buyer_id"].asString();
    }

    LOG_DEBUG << "Creating QR payment, channel: " << channel << ", order: " << orderNo;

    // Call Alipay precreate API
    alipayClient_->precreateTrade(
        payload,
        [this, orderNo, sharedCb](const Json::Value& result, const std::string& error) {
            if (!error.empty()) {
                Json::Value response;
                response["code"] = 500;
                response["message"] = "QR payment creation failed: " + error;
                (*sharedCb)(response, std::make_error_code(std::errc::io_error));
                return;
            }

            // Check Alipay response code
            std::string alipayCode = result.get("code", "").asString();
            if (alipayCode != "10000") {
                // Alipay business error
                Json::Value response;
                response["code"] = 500;
                std::string subMsg = result.get("sub_msg", "").asString();
                std::string msg = result.get("msg", "").asString();
                std::string fullMessage = "Alipay error: " + msg;
                if (!subMsg.empty()) {
                    fullMessage += " - " + subMsg;
                }
                response["message"] = fullMessage;
                response["alipay_code"] = alipayCode;
                response["alipay_sub_code"] = result.get("sub_code", "").asString();
                (*sharedCb)(response, std::make_error_code(std::errc::io_error));
                return;
            }

            // Alipay precreate response contains qr_code
            Json::Value response;
            response["code"] = 0;
            response["message"] = "QR code created successfully";

            Json::Value data;
            data["order_no"] = orderNo;

            // Extract qr_code from Alipay response
            if (result.isMember("qr_code")) {
                data["qr_code"] = result["qr_code"].asString();
            }
            if (result.isMember("out_trade_no")) {
                data["out_trade_no"] = result["out_trade_no"].asString();
            }

            response["data"] = data;
            (*sharedCb)(response, std::error_code());
        }
    );
}

void PaymentService::queryOrder(
    const std::string& orderNo,
    PaymentCallback&& callback) {

    if (!dbClient_) {
        Json::Value response;
        response["code"] = 1003;
        response["message"] = "Database client not available";
        callback(response, std::make_error_code(std::errc::io_error));
        return;
    }

    if (orderNo.empty()) {
        Json::Value response;
        response["code"] = 1001;
        response["message"] = "Missing order_no parameter";
        callback(response, std::make_error_code(std::errc::invalid_argument));
        return;
    }

    // Wrap callback in shared_ptr to prevent it from being destroyed during async operations
    auto sharedCb = std::make_shared<PaymentCallback>(std::move(callback));

    // Query order from database
    Mapper<PayOrderModel> orderMapper(dbClient_);
    auto criteria = Criteria(
        PayOrderModel::Cols::_order_no,
        CompareOperator::EQ,
        orderNo);

    orderMapper.findOne(
        criteria,
        [this, orderNo, sharedCb](const PayOrderModel &order) {
            Json::Value response;
            response["code"] = 0;
            response["message"] = "Order found";
            Json::Value data;
            data["order_no"] = order.getValueOfOrderNo();
            data["amount"] = order.getValueOfAmount();
            data["currency"] = order.getValueOfCurrency();
            data["status"] = order.getValueOfStatus();
            data["channel"] = order.getValueOfChannel();
            data["title"] = order.getValueOfTitle();
            data["user_id"] = static_cast<Json::Int64>(order.getValueOfUserId());

            const std::string channel = order.getValueOfChannel();
            LOG_DEBUG << "[PAYMENT_SERVICE] queryOrder: order_no=" << orderNo
                      << " channel=" << channel
                      << " current_status=" << data["status"].asString();

            // Query real-time status from payment channel API
            if (channel == "wechat" && wechatClient_) {
                // Query transaction from WeChat Pay
                wechatClient_->queryTransaction(
                orderNo,
                [this, orderNo, data, sharedCb](
                    const Json::Value &result, const std::string &error) {
                    if (!error.empty()) {
                        // Return database data with error header
                        Json::Value response = data;
                        response["wechat_query_error"] = error;
                        if (*sharedCb) {
                            (*sharedCb)(response, std::error_code());
                        }
                        return;
                    }

                    // Sync order status from WeChat response
                    syncOrderStatusFromWechat(
                        orderNo,
                        result,
                        [data, result, sharedCb](const std::string &status) {
                            Json::Value response = data;
                            if (!status.empty()) {
                                response["status"] = status;
                            }
                            const auto channelRefundNo = result.get("refund_id", "").asString();
                            if (!channelRefundNo.empty()) {
                                response["channel_refund_no"] = channelRefundNo;
                            }
                            response["wechat_response"] = result;
                            if (*sharedCb) {
                                (*sharedCb)(response, std::error_code());
                            }
                        });
                });
            } else if (channel == "alipay" && alipayClient_) {
                // Query trade from Alipay
                LOG_DEBUG << "[PAYMENT_SERVICE] Querying Alipay API for order " << orderNo;
                alipayClient_->queryTrade(
                    orderNo,
                    [this, orderNo, data, sharedCb](
                        const Json::Value &result, const std::string &error) {
                        if (!error.empty()) {
                            LOG_ERROR << "[PAYMENT_SERVICE] Alipay query error for " << orderNo << ": " << error;
                            // Return database data with error header
                            Json::Value response = data;
                            response["alipay_query_error"] = error;
                            if (*sharedCb) {
                                (*sharedCb)(response, std::error_code());
                            }
                            return;
                        }

                        LOG_DEBUG << "[PAYMENT_SERVICE] Alipay response for " << orderNo
                                  << " code=" << result.get("code", "?").asString()
                                  << " trade_status=" << result.get("trade_status", "?").asString();

                        // Sync order status from Alipay response
                        syncOrderStatusFromAlipay(
                            orderNo,
                            result,
                            [data, result, sharedCb, orderNo](const std::string &status) {
                                LOG_DEBUG << "[PAYMENT_SERVICE] syncOrderStatusFromAlipay returned status=" << status
                                          << " for order " << orderNo;
                                Json::Value response = data;
                                if (!status.empty()) {
                                    response["status"] = status;
                                }
                                const auto tradeNo = result.get("trade_no", "").asString();
                                if (!tradeNo.empty()) {
                                    response["trade_no"] = tradeNo;
                                }
                                response["alipay_response"] = result;
                                LOG_DEBUG << "[PAYMENT_SERVICE] Final response status=" << response["status"].asString()
                                          << " for order " << orderNo;
                                if (*sharedCb) {
                                    (*sharedCb)(response, std::error_code());
                                }
                            });
                    });
            } else {
                // Channel not supported or client not available, return database data
                LOG_DEBUG << "[PAYMENT_SERVICE] Using database data for order " << orderNo
                          << " (channel=" << channel << " has_client=" << (alipayClient_ != nullptr || wechatClient_ != nullptr) << ")";
                response["data"] = data;
                if (*sharedCb) {
                    (*sharedCb)(response, std::error_code());
                }
            }
        },
        [sharedCb](const DrogonDbException &e) {
            if (*sharedCb) {
                Json::Value response;
                response["code"] = 1004;
                response["message"] = "Order not found: " + std::string(e.base().what());
                (*sharedCb)(response, std::error_code());
            }
        });
}

void PaymentService::syncOrderStatusFromWechat(
    const std::string& orderNo,
    const Json::Value& result,
    std::function<void(const std::string& status)>&& callback) {

    const std::string tradeState = result.get("trade_state", "").asString();
    if (tradeState.empty()) {
        if (callback) {
            callback("");
        }
        return;
    }

    // Map trade state to order and payment status
    std::string orderStatus;
    std::string paymentStatus;
    pay::utils::mapTradeState(tradeState, orderStatus, paymentStatus);

    const std::string transactionId = result.get("transaction_id", "").asString();
    const std::string responsePayload = pay::utils::toJsonString(result);

    if (!dbClient_) {
        if (callback) {
            callback(orderStatus);
        }
        return;
    }

    LOG_INFO << "Sync order status from WeChat: order_no=" << orderNo
             << " trade_state=" << tradeState
             << " order_status=" << orderStatus
             << " payment_status=" << paymentStatus;

    // Find the latest payment record for this order
    Mapper<PayPaymentModel> paymentMapper(dbClient_);
    auto paymentCriteria = Criteria(
        PayPaymentModel::Cols::_order_no,
        CompareOperator::EQ,
        orderNo);

    paymentMapper.orderBy(PayPaymentModel::Cols::_created_at, SortOrder::DESC)
        .limit(1)
        .findBy(
            paymentCriteria,
            [this, orderNo, orderStatus, paymentStatus, transactionId, responsePayload, callback](
                const std::vector<PayPaymentModel> &rows) {
                if (rows.empty()) {
                    if (callback) {
                        callback(orderStatus);
                    }
                    return;
                }

                auto payment = rows.front();
                const auto paymentNo = payment.getValueOfPaymentNo();

                // Use transaction for atomic updates
                dbClient_->newTransactionAsync(
                    [this, orderNo, orderStatus, paymentStatus, transactionId, responsePayload,
                     payment, paymentNo, callback](
                        const std::shared_ptr<Transaction> &transPtr) mutable {
                        auto rollbackDone = [callback, orderStatus, transPtr](
                                                const DrogonDbException &e) {
                            LOG_ERROR << "Reconcile transaction error: " << e.base().what();
                            transPtr->rollback();
                            if (callback) {
                                callback(orderStatus);
                            }
                        };

                        auto transDb = std::static_pointer_cast<DbClient>(transPtr);

                        // If payment is already SUCCESS, only update order
                        if (payment.getValueOfStatus() == "SUCCESS") {
                            Mapper<PayOrderModel> orderMapper(transPtr);
                            auto orderCriteria = Criteria(
                                PayOrderModel::Cols::_order_no,
                                CompareOperator::EQ,
                                orderNo);
                            orderMapper.findOne(
                                orderCriteria,
                                [this, orderStatus, paymentNo, callback, transPtr, transDb](
                                    PayOrderModel order) {
                                    if (order.getValueOfStatus() != "PAID") {
                                        const auto userId = order.getValueOfUserId();
                                        const auto orderAmount = order.getValueOfAmount();
                                        const auto orderNo = order.getValueOfOrderNo();
                                        order.setStatus(orderStatus);
                                        order.setUpdatedAt(trantor::Date::now());
                                        Mapper<PayOrderModel> orderUpdater(transPtr);
                                        orderUpdater.update(
                                            order,
                                            [userId, orderNo, paymentNo, orderAmount, orderStatus, transPtr, transDb](
                                                const size_t) {
                                                if (orderStatus == "PAID") {
                                                    insertLedgerEntry(transDb, userId, orderNo,
                                                                     paymentNo, "PAYMENT", orderAmount);
                                                }
                                            },
                                            [callback, orderStatus, transPtr](
                                                const DrogonDbException &e) {
                                                LOG_ERROR << "Reconcile order update error: "
                                                          << e.base().what();
                                                transPtr->rollback();
                                                if (callback) {
                                                    callback(orderStatus);
                                                }
                                            });
                                    } else {
                                        // Order already PAID, no update needed
                                    }
                                    if (callback) {
                                        callback(orderStatus);
                                    }
                                },
                                [callback, orderStatus, transPtr](const DrogonDbException &e) {
                                    LOG_ERROR << "Reconcile order select error: " << e.base().what();
                                    transPtr->rollback();
                                    if (callback) {
                                        callback(orderStatus);
                                    }
                                });
                            return;
                        }

                        // Update payment status
                        payment.setStatus(paymentStatus);
                        payment.setChannelTradeNo(transactionId);
                        payment.setResponsePayload(responsePayload);
                        payment.setUpdatedAt(trantor::Date::now());
                        Mapper<PayPaymentModel> paymentUpdater(transPtr);
                        paymentUpdater.update(
                            payment,
                            [this, orderNo, orderStatus, paymentNo, callback, transPtr, transDb](
                                const size_t) {
                                // Update order status
                                Mapper<PayOrderModel> orderMapper(transPtr);
                                auto orderCriteria = Criteria(
                                    PayOrderModel::Cols::_order_no,
                                    CompareOperator::EQ,
                                    orderNo);
                                orderMapper.findOne(
                                    orderCriteria,
                                    [orderStatus, paymentNo, callback, transPtr, transDb](
                                        PayOrderModel order) {
                                        if (order.getValueOfStatus() == "PAID") {
                                            if (callback) {
                                                callback(orderStatus);
                                            }
                                            return;
                                        }
                                        const auto userId = order.getValueOfUserId();
                                        const auto orderAmount = order.getValueOfAmount();
                                        const auto orderNo = order.getValueOfOrderNo();
                                        order.setStatus(orderStatus);
                                        order.setUpdatedAt(trantor::Date::now());
                                        Mapper<PayOrderModel> orderUpdater(transPtr);
                                        orderUpdater.update(
                                            order,
                                            [callback, orderStatus, userId, orderNo, paymentNo,
                                             orderAmount, transPtr, transDb](const size_t) {
                                                if (orderStatus == "PAID") {
                                                    insertLedgerEntry(transDb, userId, orderNo,
                                                                     paymentNo, "PAYMENT", orderAmount);
                                                }
                                                if (callback) {
                                                    callback(orderStatus);
                                                }
                                            },
                                            [callback, orderStatus, transPtr](const DrogonDbException &e) {
                                                LOG_ERROR << "Reconcile order update error: "
                                                          << e.base().what();
                                                transPtr->rollback();
                                                if (callback) {
                                                    callback(orderStatus);
                                                }
                                            });
                                    },
                                    [callback, orderStatus, transPtr](const DrogonDbException &e) {
                                        LOG_ERROR << "Reconcile order select error: "
                                                  << e.base().what();
                                        transPtr->rollback();
                                        if (callback) {
                                            callback(orderStatus);
                                        }
                                    });
                            },
                            rollbackDone);
                    });
            },
            [](const DrogonDbException &e) {
                LOG_ERROR << "Reconcile payment select error: " << e.base().what();
            });
}

void PaymentService::syncOrderStatusFromAlipay(
    const std::string& orderNo,
    const Json::Value& result,
    std::function<void(const std::string& status)>&& callback) {

    const std::string responseCode = result.get("code", "").asString();
    if (responseCode != "10000") {
        // Alipay API call failed or trade not found
        if (callback) {
            callback("");
        }
        return;
    }

    const std::string tradeStatus = result.get("trade_status", "").asString();
    if (tradeStatus.empty()) {
        if (callback) {
            callback("");
        }
        return;
    }

    // Map Alipay trade_status to order and payment status
    std::string orderStatus;
    std::string paymentStatus;

    if (tradeStatus == "TRADE_SUCCESS" || tradeStatus == "TRADE_FINISHED") {
        orderStatus = "PAID";
        paymentStatus = "SUCCESS";
    } else if (tradeStatus == "WAIT_BUYER_PAY") {
        orderStatus = "PAYING";
        paymentStatus = "PROCESSING";
    } else if (tradeStatus == "TRADE_CLOSED") {
        orderStatus = "FAILED";
        paymentStatus = "FAILED";
    } else {
        // Unknown status
        LOG_WARN << "Unknown Alipay trade_status: " << tradeStatus << " for order " << orderNo;
        if (callback) {
            callback("");
        }
        return;
    }

    const std::string transactionId = result.get("trade_no", "").asString();
    const std::string responsePayload = pay::utils::toJsonString(result);

    if (!dbClient_) {
        if (callback) {
            callback(orderStatus);
        }
        return;
    }

    LOG_DEBUG << "Sync order status from Alipay: order_no=" << orderNo
              << " trade_status=" << tradeStatus
              << " order_status=" << orderStatus
              << " payment_status=" << paymentStatus;

    // Find the latest payment record for this order
    Mapper<PayPaymentModel> paymentMapper(dbClient_);
    auto paymentCriteria = Criteria(
        PayPaymentModel::Cols::_order_no,
        CompareOperator::EQ,
        orderNo);

    paymentMapper.orderBy(PayPaymentModel::Cols::_created_at, SortOrder::DESC)
        .limit(1)
        .findBy(
            paymentCriteria,
            [this, orderNo, orderStatus, paymentStatus, transactionId, responsePayload, callback](
                const std::vector<PayPaymentModel> &rows) {
                if (rows.empty()) {
                    if (callback) {
                        callback(orderStatus);
                    }
                    return;
                }

                auto payment = rows.front();
                const auto paymentNo = payment.getValueOfPaymentNo();

                // Use transaction for atomic updates
                dbClient_->newTransactionAsync(
                    [this, orderNo, orderStatus, paymentStatus, transactionId, responsePayload,
                     payment, paymentNo, callback](
                        const std::shared_ptr<Transaction> &transPtr) mutable {
                        auto rollbackDone = [callback, orderStatus, transPtr](
                                                const DrogonDbException &e) {
                            LOG_ERROR << "Alipay reconcile transaction error: " << e.base().what();
                            transPtr->rollback();
                            if (callback) {
                                callback(orderStatus);
                            }
                        };

                        auto transDb = std::static_pointer_cast<DbClient>(transPtr);

                        // If payment is already SUCCESS, only update order
                        if (payment.getValueOfStatus() == "SUCCESS") {
                            Mapper<PayOrderModel> orderMapper(transPtr);
                            auto orderCriteria = Criteria(
                                PayOrderModel::Cols::_order_no,
                                CompareOperator::EQ,
                                orderNo);
                            orderMapper.findOne(
                                orderCriteria,
                                [this, orderStatus, paymentNo, callback, transPtr, transDb](
                                    PayOrderModel order) {
                                    if (order.getValueOfStatus() != "PAID") {
                                        const auto userId = order.getValueOfUserId();
                                        const auto orderAmount = order.getValueOfAmount();
                                        const auto orderNo = order.getValueOfOrderNo();
                                        order.setStatus(orderStatus);
                                        order.setUpdatedAt(trantor::Date::now());
                                        Mapper<PayOrderModel> orderUpdater(transPtr);
                                        orderUpdater.update(
                                            order,
                                            [userId, orderNo, paymentNo, orderAmount, orderStatus, transPtr, transDb](
                                                const size_t) {
                                                if (orderStatus == "PAID") {
                                                    insertLedgerEntry(transDb, userId, orderNo,
                                                                     paymentNo, "PAYMENT", orderAmount);
                                                }
                                            },
                                            [callback, orderStatus, transPtr](
                                                const DrogonDbException &e) {
                                                LOG_ERROR << "Alipay reconcile order update error: "
                                                          << e.base().what();
                                                transPtr->rollback();
                                                if (callback) {
                                                    callback(orderStatus);
                                                }
                                            });
                                    } else {
                                        // Order already PAID, no update needed
                                    }
                                    if (callback) {
                                        callback(orderStatus);
                                    }
                                },
                                [callback, orderStatus, transPtr](const DrogonDbException &e) {
                                    LOG_ERROR << "Alipay reconcile order select error: " << e.base().what();
                                    transPtr->rollback();
                                    if (callback) {
                                        callback(orderStatus);
                                    }
                                });
                            return;
                        }

                        // Update payment status
                        payment.setStatus(paymentStatus);
                        payment.setChannelTradeNo(transactionId);
                        payment.setResponsePayload(responsePayload);
                        payment.setUpdatedAt(trantor::Date::now());
                        Mapper<PayPaymentModel> paymentUpdater(transPtr);
                        paymentUpdater.update(
                            payment,
                            [this, orderNo, orderStatus, paymentNo, callback, transPtr, transDb](
                                const size_t) {
                                // Update order status
                                Mapper<PayOrderModel> orderMapper(transPtr);
                                auto orderCriteria = Criteria(
                                    PayOrderModel::Cols::_order_no,
                                    CompareOperator::EQ,
                                    orderNo);
                                orderMapper.findOne(
                                    orderCriteria,
                                    [orderStatus, paymentNo, callback, transPtr, transDb](
                                        PayOrderModel order) {
                                        if (order.getValueOfStatus() == "PAID") {
                                            if (callback) {
                                                callback(orderStatus);
                                            }
                                            return;
                                        }
                                        const auto userId = order.getValueOfUserId();
                                        const auto orderAmount = order.getValueOfAmount();
                                        const auto orderNo = order.getValueOfOrderNo();
                                        order.setStatus(orderStatus);
                                        order.setUpdatedAt(trantor::Date::now());
                                        Mapper<PayOrderModel> orderUpdater(transPtr);
                                        orderUpdater.update(
                                            order,
                                            [callback, orderStatus, userId, orderNo, paymentNo,
                                             orderAmount, transPtr, transDb](const size_t) {
                                                if (orderStatus == "PAID") {
                                                    insertLedgerEntry(transDb, userId, orderNo,
                                                                     paymentNo, "PAYMENT", orderAmount);
                                                }
                                                if (callback) {
                                                    callback(orderStatus);
                                                }
                                            },
                                            [callback, orderStatus, transPtr](const DrogonDbException &e) {
                                                LOG_ERROR << "Alipay reconcile order update error: "
                                                          << e.base().what();
                                                transPtr->rollback();
                                                if (callback) {
                                                    callback(orderStatus);
                                                }
                                            });
                                    },
                                    [callback, orderStatus, transPtr](const DrogonDbException &e) {
                                        LOG_ERROR << "Alipay reconcile order select error: "
                                                  << e.base().what();
                                        transPtr->rollback();
                                        if (callback) {
                                            callback(orderStatus);
                                        }
                                    });
                            },
                            rollbackDone);
                    });
            },
            [](const DrogonDbException &e) {
                LOG_ERROR << "Alipay reconcile payment select error: " << e.base().what();
            });
}

void PaymentService::reconcileSummary(
    const std::string& date,
    PaymentCallback&& callback) {

    if (!dbClient_) {
        Json::Value response;
        response["code"] = 1003;
        response["message"] = "Database client not available";
        callback(response, std::make_error_code(std::errc::io_error));
        return;
    }

    auto responded = std::make_shared<std::atomic<bool>>(false);
    auto pending = std::make_shared<std::atomic<int>>(2);
    auto summary = std::make_shared<Json::Value>();
    (*summary)["paying_orders"] = 0;
    (*summary)["refunding_refunds"] = 0;
    (*summary)["oldest_paying_updated"] = "";
    (*summary)["oldest_refund_updated"] = "";

    // Wrap callback in shared_ptr to prevent it from being destroyed during async operations
    auto sharedCb = std::make_shared<PaymentCallback>(std::move(callback));

    auto finishIfReady = [sharedCb, responded, pending, summary]() {
        if (pending->fetch_sub(1) != 1) {
            return;
        }
        if (responded->exchange(true)) {
            return;
        }
        if (*sharedCb) {
            Json::Value response;
            response["code"] = 0;
            response["message"] = "Reconciliation summary";
            response["data"] = *summary;
            (*sharedCb)(response, std::error_code());
        }
    };

    // Query paying orders
    dbClient_->execSqlAsync(
        "SELECT COUNT(*) AS cnt, MIN(updated_at) AS oldest_updated "
        "FROM pay_order WHERE status = $1",
        [summary, finishIfReady](const Result &r) {
            if (!r.empty()) {
                const auto &row = r.front();
                (*summary)["paying_orders"] = row["cnt"].as<int64_t>();
                if (!row["oldest_updated"].isNull()) {
                    (*summary)["oldest_paying_updated"] =
                        row["oldest_updated"].as<std::string>();
                }
            }
            finishIfReady();
        },
        [sharedCb, responded](const DrogonDbException &e) {
            if (responded->exchange(true)) {
                return;
            }
            if (*sharedCb) {
                Json::Value response;
                response["code"] = 1003;
                response["message"] = "Database error: " + std::string(e.base().what());
                (*sharedCb)(response, std::make_error_code(std::errc::io_error));
            }
        },
        "PAYING");

    // Query refunding refunds
    dbClient_->execSqlAsync(
        "SELECT COUNT(*) AS cnt, MIN(updated_at) AS oldest_updated "
        "FROM pay_refund WHERE status IN ($1, $2)",
        [summary, finishIfReady](const Result &r) {
            if (!r.empty()) {
                const auto &row = r.front();
                (*summary)["refunding_refunds"] = row["cnt"].as<int64_t>();
                if (!row["oldest_updated"].isNull()) {
                    (*summary)["oldest_refund_updated"] =
                        row["oldest_updated"].as<std::string>();
                }
            }
            finishIfReady();
        },
        [sharedCb, responded](const DrogonDbException &e) {
            if (responded->exchange(true)) {
                return;
            }
            if (*sharedCb) {
                Json::Value response;
                response["code"] = 1003;
                response["message"] = "Database error: " + std::string(e.base().what());
                (*sharedCb)(response, std::make_error_code(std::errc::io_error));
            }
        },
        "REFUND_INIT",
        "REFUNDING");
}

std::string PaymentService::generatePaymentNo() {
    // Generate unique payment number
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 99999999);

    std::ostringstream oss;
    time_t now = std::time(nullptr);
    struct tm tmInfo;
    localtime_s(&tmInfo, &now);
    oss << "PAY" << std::put_time(&tmInfo, "%Y%m%d%H%M%S");
    oss << std::setfill('0') << std::setw(8) << dis(gen);

    return oss.str();
}
