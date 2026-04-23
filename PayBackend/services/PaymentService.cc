#include "PaymentService.h"
#include "../models/PayOrder.h"
#include "../models/PayPayment.h"
#include "../models/PayLedger.h"
#include "../models/PayIdempotency.h"
#include "../utils/PayUtils.h"
#include <drogon/drogon.h>
#include <random>
#include <sstream>

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
        idemp.setExpiresAt(expiresAt);

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

    // Build WeChat Pay request payload
    Json::Value payload;
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

    const std::string requestPayload = pay::utils::toJsonString(payload);

    // Insert order into database
    orderMapper.insert(
        order,
        [this, request, paymentNo, payload, requestPayload, callback](const PayOrderModel &) {
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
                [this, request, paymentNo, payload, callback](const PayPaymentModel &) {
                    // Call WeChat Pay API to create transaction
                    wechatClient_->createTransactionNative(
                        payload,
                        [this, request, paymentNo, callback](
                            const Json::Value &result, const std::string &error) {

                            if (!error.empty()) {
                                // Handle WeChat Pay error
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
                                    [this, errPayload, request](PayPaymentModel payment) {
                                        payment.setStatus("FAIL");
                                        payment.setResponsePayload(errPayload);
                                        payment.setUpdatedAt(trantor::Date::now());
                                        Mapper<PayPaymentModel> paymentUpdater(dbClient_);
                                        paymentUpdater.update(
                                            payment,
                                            [this, request](const size_t) {
                                                // Update order status to FAILED
                                                Mapper<PayOrderModel> orderMapper(dbClient_);
                                                auto orderCriteria = Criteria(
                                                    PayOrderModel::Cols::_order_no,
                                                    CompareOperator::EQ,
                                                    request.orderNo);
                                                orderMapper.findOne(
                                                    orderCriteria,
                                                    [this](PayOrderModel order) {
                                                        order.setStatus("FAILED");
                                                        order.setUpdatedAt(trantor::Date::now());
                                                        Mapper<PayOrderModel> orderUpdater(dbClient_);
                                                        orderUpdater.update(
                                                            order,
                                                            [](const size_t) {},
                                                            [](const DrogonDbException &) {});
                                                    },
                                                    [](const DrogonDbException &) {});
                                            },
                                            [](const DrogonDbException &) {});
                                    },
                                    [](const DrogonDbException &) {});

                                // Return error response
                                Json::Value response;
                                response["code"] = 1002;
                                response["message"] = "WeChat Pay error: " + error;
                                callback(response, std::error_code());
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
                                [this, request, paymentNo, result, responsePayload, callback](
                                    PayPaymentModel payment) {
                                    payment.setStatus("PROCESSING");
                                    payment.setResponsePayload(responsePayload);
                                    payment.setUpdatedAt(trantor::Date::now());
                                    Mapper<PayPaymentModel> paymentUpdater(dbClient_);
                                    paymentUpdater.update(
                                        payment,
                                        [this, request, paymentNo, result, callback](
                                            const size_t) {
                                            // Update order status to PAYING
                                            Mapper<PayOrderModel> orderMapper(dbClient_);
                                            auto orderCriteria = Criteria(
                                                PayOrderModel::Cols::_order_no,
                                                CompareOperator::EQ,
                                                request.orderNo);
                                            orderMapper.findOne(
                                                orderCriteria,
                                                [this, request, paymentNo, result, callback](
                                                    PayOrderModel order) {
                                                    order.setStatus("PAYING");
                                                    order.setUpdatedAt(trantor::Date::now());
                                                    Mapper<PayOrderModel> orderUpdater(dbClient_);
                                                    orderUpdater.update(
                                                        order,
                                                        [this, request, paymentNo, result, callback](
                                                            const size_t) {
                                                            // Build success response
                                                            Json::Value response;
                                                            response["code"] = 0;
                                                            response["message"] = "Payment created successfully";
                                                            Json::Value data;
                                                            data["order_no"] = request.orderNo;
                                                            data["payment_no"] = paymentNo;
                                                            data["status"] = "PAYING";

                                                            // Add WeChat response details
                                                            const auto codeUrl = result.get("code_url", "").asString();
                                                            if (!codeUrl.empty()) {
                                                                data["code_url"] = codeUrl;
                                                            }
                                                            const auto prepayId = result.get("prepay_id", "").asString();
                                                            if (!prepayId.empty()) {
                                                                data["prepay_id"] = prepayId;
                                                            }
                                                            data["wechat_response"] = result;

                                                            response["data"] = data;
                                                            callback(response, std::error_code());
                                                        },
                                                        [callback](const DrogonDbException &e) {
                                                            Json::Value response;
                                                            response["code"] = 1003;
                                                            response["message"] = "Database error: " + std::string(e.base().what());
                                                            callback(response, std::error_code());
                                                        });
                                                },
                                                [callback](const DrogonDbException &e) {
                                                    Json::Value response;
                                                    response["code"] = 1003;
                                                    response["message"] = "Database error: " + std::string(e.base().what());
                                                    callback(response, std::error_code());
                                                });
                                        },
                                        [callback](const DrogonDbException &e) {
                                            Json::Value response;
                                            response["code"] = 1003;
                                            response["message"] = "Database error: " + std::string(e.base().what());
                                            callback(response, std::error_code());
                                        });
                                },
                                [callback](const DrogonDbException &e) {
                                    Json::Value response;
                                    response["code"] = 1003;
                                    response["message"] = "Database error: " + std::string(e.base().what());
                                    callback(response, std::error_code());
                                });
                        });
                },
                [callback](const DrogonDbException &e) {
                    Json::Value response;
                    response["code"] = 1003;
                    response["message"] = "Database error: " + std::string(e.base().what());
                    callback(response, std::make_error_code(std::errc::io_error));
                });
        },
        [callback](const DrogonDbException &e) {
            Json::Value response;
            response["code"] = 1003;
            response["message"] = "Database error: " + std::string(e.base().what());
            callback(response, std::make_error_code(std::errc::io_error));
        });
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

    // Query order from database
    Mapper<PayOrderModel> orderMapper(dbClient_);
    auto criteria = Criteria(
        PayOrderModel::Cols::_order_no,
        CompareOperator::EQ,
        orderNo);

    orderMapper.findOne(
        criteria,
        [this, orderNo, callback](const PayOrderModel &order) {
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

            // If not WeChat channel or WeChat client not available, return database data
            if (!wechatClient_ || order.getValueOfChannel() != "wechat") {
                response["data"] = data;
                callback(response, std::error_code());
                return;
            }

            // Query transaction from WeChat Pay
            wechatClient_->queryTransaction(
                orderNo,
                [this, orderNo, data, callback](
                    const Json::Value &result, const std::string &error) {
                    if (!error.empty()) {
                        // Return database data with error header
                        Json::Value response = data;
                        response["wechat_query_error"] = error;
                        callback(response, std::error_code());
                        return;
                    }

                    // Sync order status from WeChat response
                    syncOrderStatusFromWechat(
                        orderNo,
                        result,
                        [data, result, callback](const std::string &status) {
                            Json::Value response = data;
                            if (!status.empty()) {
                                response["status"] = status;
                            }
                            const auto channelRefundNo = result.get("refund_id", "").asString();
                            if (!channelRefundNo.empty()) {
                                response["channel_refund_no"] = channelRefundNo;
                            }
                            response["wechat_response"] = result;
                            callback(response, std::error_code());
                        });
                });
        },
        [callback](const DrogonDbException &e) {
            Json::Value response;
            response["code"] = 1004;
            response["message"] = "Order not found: " + std::string(e.base().what());
            callback(response, std::error_code());
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

    auto finishIfReady = [callback, responded, pending, summary]() {
        if (pending->fetch_sub(1) != 1) {
            return;
        }
        if (responded->exchange(true)) {
            return;
        }
        Json::Value response;
        response["code"] = 0;
        response["message"] = "Reconciliation summary";
        response["data"] = *summary;
        callback(response, std::error_code());
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
        [callback, responded](const DrogonDbException &e) {
            if (responded->exchange(true)) {
                return;
            }
            Json::Value response;
            response["code"] = 1003;
            response["message"] = "Database error: " + std::string(e.base().what());
            callback(response, std::make_error_code(std::errc::io_error));
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
        [callback, responded](const DrogonDbException &e) {
            if (responded->exchange(true)) {
                return;
            }
            Json::Value response;
            response["code"] = 1003;
            response["message"] = "Database error: " + std::string(e.base().what());
            callback(response, std::make_error_code(std::errc::io_error));
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
