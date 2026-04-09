#include "CallbackService.h"
#include "../utils/PayUtils.h"
#include "../models/PayOrder.h"
#include "../models/PayPayment.h"
#include "../models/PayRefund.h"
#include "../models/PayCallback.h"
#include "../models/PayIdempotency.h"
#include "../models/PayLedger.h"
#include <drogon/drogon.h>
#include <sstream>

using namespace drogon;
using PayOrderModel = drogon_model::pay_test::PayOrder;
using PayPaymentModel = drogon_model::pay_test::PayPayment;
using PayRefundModel = drogon_model::pay_test::PayRefund;
using PayCallbackModel = drogon_model::pay_test::PayCallback;
using PayIdempotencyModel = drogon_model::pay_test::PayIdempotency;
using PayLedgerModel = drogon_model::pay_test::PayLedger;

namespace {

void insertLedgerEntry(
    const std::shared_ptr<drogon::orm::DbClient> &dbClient,
    int64_t userId,
    const std::string &orderNo,
    const std::string &paymentNo,
    const std::string &entryType,
    const std::string &amount)
{
    if (!dbClient)
    {
        LOG_ERROR << "DbClient is null in insertLedgerEntry";
        return;
    }

    PayLedgerModel ledger;
    ledger.setUserId(userId);
    ledger.setOrderNo(orderNo);
    ledger.setPaymentNo(paymentNo);
    ledger.setEntryType(entryType);
    ledger.setAmount(amount);
    ledger.setCreatedAt(trantor::Date::now());

    try
    {
        drogon::orm::Mapper<PayLedgerModel> mapper(dbClient);
        mapper.insert(ledger);
        LOG_DEBUG << "Ledger entry inserted: " << entryType << " " << amount;
    }
    catch (const drogon::orm::DrogonDbException &e)
    {
        LOG_ERROR << "Failed to insert ledger entry: " << e.base().what();
    }
}

} // namespace

CallbackService::CallbackService(
    std::shared_ptr<WechatPayClient> wechatClient,
    std::shared_ptr<drogon::orm::DbClient> dbClient)
    : wechatClient_(wechatClient), dbClient_(dbClient) {
}

void CallbackService::handlePaymentCallback(
    const std::string& body,
    const std::string& signature,
    const std::string& timestamp,
    const std::string& nonce,
    const std::string& serialNo,
    CallbackResult&& callback) {

    auto respond = [callback](const Json::Value &result, const std::string &errorMsg) {
        if (!errorMsg.empty())
        {
            Json::Value error;
            error["code"] = "FAIL";
            error["message"] = errorMsg;
            callback(error, std::error_code());
            return;
        }
        callback(result, std::error_code());
    };

    // Verify signature first
    if (!verifySignature(body, signature, timestamp, nonce, serialNo))
    {
        Json::Value error;
        error["code"] = "FAIL";
        error["message"] = "signature verification failed";
        respond(error, "signature verification failed");
        return;
    }

    // Parse callback body
    Json::CharReaderBuilder builder;
    Json::Value notifyJson;
    std::string parseErrors;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    if (!reader->parse(body.data(), body.data() + body.size(), &notifyJson, &parseErrors))
    {
        Json::Value error;
        error["code"] = "FAIL";
        error["message"] = "invalid json";
        respond(error, "invalid json");
        return;
    }

    // Validate event_type
    const std::string eventType = notifyJson.get("event_type", "").asString();
    if (eventType.empty())
    {
        Json::Value error;
        error["code"] = "FAIL";
        error["message"] = "missing event_type";
        respond(error, "missing event_type");
        return;
    }

    if (eventType.rfind("TRANSACTION.", 0) != 0)
    {
        Json::Value error;
        error["code"] = "FAIL";
        error["message"] = "invalid transaction event_type";
        respond(error, "invalid transaction event_type");
        return;
    }

    // Validate resource
    if (!notifyJson.isMember("resource"))
    {
        Json::Value error;
        error["code"] = "FAIL";
        error["message"] = "missing resource";
        respond(error, "missing resource");
        return;
    }

    const auto &resource = notifyJson["resource"];
    const std::string resourceType = notifyJson.get("resource_type", "").asString();
    if (resourceType.empty() || resourceType != "encrypt-resource")
    {
        Json::Value error;
        error["code"] = "FAIL";
        error["message"] = "unsupported resource_type";
        respond(error, "unsupported resource_type");
        return;
    }

    const std::string algorithm = resource.get("algorithm", "").asString();
    if (algorithm.empty() || algorithm != "AEAD_AES_256_GCM")
    {
        Json::Value error;
        error["code"] = "FAIL";
        error["message"] = "unsupported resource algorithm";
        respond(error, "unsupported resource algorithm");
        return;
    }

    const std::string ciphertext = resource.get("ciphertext", "").asString();
    const std::string nonceStr = resource.get("nonce", "").asString();
    const std::string associatedData = resource.get("associated_data", "").asString();
    if (ciphertext.empty() || nonceStr.empty())
    {
        Json::Value error;
        error["code"] = "FAIL";
        error["message"] = "invalid resource";
        respond(error, "invalid resource");
        return;
    }

    if (associatedData != "transaction")
    {
        Json::Value error;
        error["code"] = "FAIL";
        error["message"] = "invalid transaction associated_data";
        respond(error, "invalid transaction associated_data");
        return;
    }

    // Decrypt resource
    std::string plaintext;
    std::string decryptError;
    if (!wechatClient_->decryptResource(ciphertext, nonceStr, associatedData, plaintext, decryptError))
    {
        Json::Value error;
        error["code"] = "FAIL";
        error["message"] = decryptError;
        respond(error, decryptError);
        return;
    }

    // Parse decrypted JSON
    Json::Value plainJson;
    Json::CharReaderBuilder plainBuilder;
    std::string plainErrors;
    std::unique_ptr<Json::CharReader> plainReader(plainBuilder.newCharReader());
    if (!plainReader->parse(plaintext.data(), plaintext.data() + plaintext.size(), &plainJson, &plainErrors))
    {
        Json::Value error;
        error["code"] = "FAIL";
        error["message"] = "invalid resource json";
        respond(error, "invalid resource json");
        return;
    }

    // Validate appid and mchid
    const std::string appId = plainJson.get("appid", "").asString();
    const std::string mchId = plainJson.get("mchid", "").asString();
    if (!appId.empty() && wechatClient_ && appId != wechatClient_->getAppId())
    {
        Json::Value error;
        error["code"] = "FAIL";
        error["message"] = "appid mismatch";
        respond(error, "appid mismatch");
        return;
    }
    if (!mchId.empty() && wechatClient_ && mchId != wechatClient_->getMchId())
    {
        Json::Value error;
        error["code"] = "FAIL";
        error["message"] = "mchid mismatch";
        respond(error, "mchid mismatch");
        return;
    }

    // Extract payment details
    const std::string orderNo = plainJson.get("out_trade_no", "").asString();
    const std::string transactionId = plainJson.get("transaction_id", "").asString();
    const std::string tradeState = plainJson.get("trade_state", "").asString();

    if (orderNo.empty() || tradeState.empty())
    {
        Json::Value error;
        error["code"] = "FAIL";
        error["message"] = "missing out_trade_no/trade_state";
        respond(error, "missing out_trade_no/trade_state");
        return;
    }

    const bool tradeStateValid =
        tradeState == "SUCCESS" || tradeState == "USERPAYING" ||
        tradeState == "NOTPAY" || tradeState == "CLOSED" ||
        tradeState == "REVOKED" || tradeState == "REFUND";
    if (!tradeStateValid)
    {
        Json::Value error;
        error["code"] = "FAIL";
        error["message"] = "invalid trade_state";
        respond(error, "invalid trade_state");
        return;
    }

    if (tradeState == "SUCCESS" && transactionId.empty())
    {
        Json::Value error;
        error["code"] = "FAIL";
        error["message"] = "missing transaction_id";
        respond(error, "missing transaction_id");
        return;
    }

    // Idempotency check
    std::string idempotencyKey = notifyJson.get("id", "").asString();
    if (idempotencyKey.empty())
    {
        idempotencyKey = orderNo + ":" + tradeState;
    }

    auto cbPtr = std::make_shared<CallbackResult>(std::move(callback));

    auto proceedWithDb = [this,
                          cbPtr,
                          idempotencyKey,
                          orderNo,
                          transactionId,
                          tradeState,
                          plaintext,
                          body,
                          signature,
                          serialNo,
                          plainJson]() {
        drogon::orm::Mapper<PayIdempotencyModel> idempMapper(dbClient_);
        auto idempCriteria =
            drogon::orm::Criteria(PayIdempotencyModel::Cols::_idempotency_key,
                                  drogon::orm::CompareOperator::EQ,
                                  idempotencyKey);
        idempMapper.findOne(
            idempCriteria,
            [cbPtr](const PayIdempotencyModel &) {
                // Already processed - return success
                Json::Value ok;
                ok["code"] = "SUCCESS";
                ok["message"] = "OK";
                (*cbPtr)(ok, std::error_code());
            },
            [this,
             cbPtr,
             idempotencyKey,
             orderNo,
             transactionId,
             tradeState,
             plaintext,
             body,
             signature,
             serialNo,
             plainJson](const drogon::orm::DrogonDbException &) {
                const std::string requestHash = drogon::utils::getMd5(body);
                PayIdempotencyModel idemp;
                idemp.setIdempotencyKey(idempotencyKey);
                idemp.setRequestHash(requestHash);
                idemp.setResponseSnapshot(plaintext);
                const auto now = trantor::Date::now();
                const auto expiresAt = trantor::Date(
                    now.microSecondsSinceEpoch() +
                    static_cast<int64_t>(7) * 24 * 60 * 60 * 1000000);
                idemp.setExpiresAt(expiresAt);

                dbClient_->newTransactionAsync(
                    [this,
                     cbPtr,
                     orderNo,
                     transactionId,
                     tradeState,
                     plaintext,
                     body,
                     signature,
                     serialNo,
                     plainJson,
                     idemp](const std::shared_ptr<drogon::orm::Transaction> &transPtr) mutable {
                        if (!transPtr)
                        {
                            Json::Value error;
                            error["code"] = "FAIL";
                            error["message"] = "db transaction unavailable";
                            (*cbPtr)(error, std::error_code());
                            return;
                        }
                        auto respondDbError =
                            [cbPtr](const drogon::orm::DrogonDbException &e) {
                                Json::Value error;
                                error["code"] = "FAIL";
                                error["message"] = std::string("db error: ") + e.base().what();
                                (*cbPtr)(error, std::error_code());
                            };

                        drogon::orm::Mapper<PayIdempotencyModel> idempInsert(transPtr);
                        idempInsert.insert(
                            idemp,
                            [this,
                             cbPtr,
                             orderNo,
                             transactionId,
                             tradeState,
                             plaintext,
                             body,
                             signature,
                             serialNo,
                             plainJson,
                             transPtr,
                             respondDbError](const PayIdempotencyModel &) {
                                drogon::orm::Mapper<PayPaymentModel> paymentMapper(transPtr);
                                auto paymentCriteria =
                                    drogon::orm::Criteria(
                                        PayPaymentModel::Cols::_order_no,
                                        drogon::orm::CompareOperator::EQ,
                                        orderNo);
                                paymentMapper.orderBy(PayPaymentModel::Cols::_created_at,
                                                      drogon::orm::SortOrder::DESC)
                                    .limit(1)
                                    .findBy(
                                        paymentCriteria,
                                        [this,
                                         cbPtr,
                                         orderNo,
                                         transactionId,
                                         tradeState,
                                         plaintext,
                                         body,
                                         signature,
                                         serialNo,
                                         plainJson,
                                         transPtr,
                                         respondDbError](const std::vector<PayPaymentModel> &rows) {
                                            if (rows.empty())
                                            {
                                                transPtr->rollback();
                                                Json::Value error;
                                                error["code"] = "FAIL";
                                                error["message"] = "payment not found";
                                                (*cbPtr)(error, std::error_code());
                                                return;
                                            }

                                            auto payment = rows.front();
                                            const std::string paymentNo =
                                                payment.getValueOfPaymentNo();
                                            const std::string orderAmount =
                                                payment.getValueOfAmount();

                                            drogon::orm::Mapper<PayOrderModel> orderMapper(transPtr);
                                            auto orderCriteria =
                                                drogon::orm::Criteria(
                                                    PayOrderModel::Cols::_order_no,
                                                    drogon::orm::CompareOperator::EQ,
                                                    orderNo);
                                            orderMapper.findOne(
                                                orderCriteria,
                                                [this,
                                                 cbPtr,
                                                 orderNo,
                                                 paymentNo,
                                                 orderAmount,
                                                 transactionId,
                                                 tradeState,
                                                 plaintext,
                                                 body,
                                                 signature,
                                                 serialNo,
                                                 plainJson,
                                                 transPtr,
                                                 respondDbError,
                                                 payment](PayOrderModel order) mutable {
                                                    const std::string orderCurrency =
                                                        order.getValueOfCurrency();
                                                    const auto &amountJson =
                                                        plainJson["amount"];
                                                    const std::string notifyCurrency =
                                                        amountJson.get("currency", "")
                                                            .asString();
                                                    const int64_t notifyTotalFen =
                                                        amountJson.get("total", 0)
                                                            .asInt64();
                                                    int64_t orderTotalFen = 0;
                                                    if (!pay::utils::parseAmountToFen(
                                                            orderAmount, orderTotalFen) ||
                                                        notifyTotalFen <= 0)
                                                    {
                                                        transPtr->rollback();
                                                        Json::Value error;
                                                        error["code"] = "FAIL";
                                                        error["message"] = "invalid amount in callback";
                                                        (*cbPtr)(error, std::error_code());
                                                        return;
                                                    }
                                                    if (!notifyCurrency.empty() &&
                                                        notifyCurrency != orderCurrency)
                                                    {
                                                        transPtr->rollback();
                                                        Json::Value error;
                                                        error["code"] = "FAIL";
                                                        error["message"] = "currency mismatch";
                                                        (*cbPtr)(error, std::error_code());
                                                        return;
                                                    }
                                                    if (notifyTotalFen != orderTotalFen)
                                                    {
                                                        transPtr->rollback();
                                                        Json::Value error;
                                                        error["code"] = "FAIL";
                                                        error["message"] = "amount mismatch";
                                                        (*cbPtr)(error, std::error_code());
                                                        return;
                                                    }

                                                    std::string orderStatus;
                                                    std::string paymentStatus;
                                                    pay::utils::mapTradeState(tradeState,
                                                                          orderStatus,
                                                                          paymentStatus);

                                                    PayCallbackModel callbackRow;
                                                    callbackRow.setPaymentNo(paymentNo);
                                                    callbackRow.setRawBody(body);
                                                    callbackRow.setSignature(signature);
                                                    callbackRow.setSerialNo(serialNo);
                                                    callbackRow.setVerified(true);
                                                    callbackRow.setProcessed(true);
                                                    callbackRow.setReceivedAt(trantor::Date::now());

                                                    drogon::orm::Mapper<PayCallbackModel>
                                                        callbackMapper(transPtr);
                                                    callbackMapper.insert(
                                                        callbackRow,
                                                        [this,
                                                         cbPtr,
                                                         orderNo,
                                                         paymentNo,
                                                         orderStatus,
                                                         paymentStatus,
                                                         transactionId,
                                                         plaintext,
                                                         transPtr,
                                                         respondDbError,
                                                         payment,
                                                         order](const PayCallbackModel &) mutable {
                                                            auto transDb =
                                                                std::static_pointer_cast<drogon::orm::DbClient>(transPtr);

                                                            payment.setStatus(paymentStatus);
                                                            payment.setChannelTradeNo(transactionId);
                                                            payment.setResponsePayload(plaintext);
                                                            payment.setUpdatedAt(trantor::Date::now());
                                                            drogon::orm::Mapper<PayPaymentModel>
                                                                paymentUpdater(transPtr);
                                                            paymentUpdater.update(
                                                                payment,
                                                                [cbPtr,
                                                                 orderStatus,
                                                                 paymentNo,
                                                                 transDb,
                                                                 orderNo,
                                                                 order,
                                                                 transPtr](const size_t) mutable {
                                                                    order.setStatus(orderStatus);
                                                                    order.setUpdatedAt(trantor::Date::now());
                                                                    drogon::orm::Mapper<PayOrderModel>
                                                                        orderUpdater(transPtr);
                                                                    orderUpdater.update(
                                                                        order,
                                                                        [cbPtr,
                                                                         orderStatus,
                                                                         paymentNo,
                                                                         transDb,
                                                                         orderNo,
                                                                         order,
                                                                         transPtr](const size_t) {
                                                                            if (orderStatus == "PAID")
                                                                            {
                                                                                insertLedgerEntry(
                                                                                    transDb,
                                                                                    order.getValueOfUserId(),
                                                                                    orderNo,
                                                                                    paymentNo,
                                                                                    "PAYMENT",
                                                                                    order.getValueOfAmount());
                                                                            }
                                                                            Json::Value ok;
                                                                            ok["code"] = "SUCCESS";
                                                                            ok["message"] = "OK";
                                                                            (*cbPtr)(ok, std::error_code());
                                                                        },
                                                                        [cbPtr](const drogon::orm::DrogonDbException &e) {
                                                                            Json::Value error;
                                                                            error["code"] = "FAIL";
                                                                            error["message"] = std::string("db error: ") + e.base().what();
                                                                            (*cbPtr)(error, std::error_code());
                                                                        });
                                                                },
                                                                respondDbError);
                                                        },
                                                        respondDbError);
                                                },
                                                respondDbError);
                                        },
                                        respondDbError);
                            },
                            respondDbError);
                    });
            });
    };

    // Skip Redis idempotency for now (simpler implementation)
    proceedWithDb();
}

void CallbackService::handleRefundCallback(
    const std::string& body,
    const std::string& signature,
    const std::string& timestamp,
    const std::string& nonce,
    const std::string& serialNo,
    CallbackResult&& callback) {

    auto respond = [callback](const Json::Value &result, const std::string &errorMsg) {
        if (!errorMsg.empty())
        {
            Json::Value error;
            error["code"] = "FAIL";
            error["message"] = errorMsg;
            callback(error, std::error_code());
            return;
        }
        callback(result, std::error_code());
    };

    // Verify signature first
    if (!verifySignature(body, signature, timestamp, nonce, serialNo))
    {
        Json::Value error;
        error["code"] = "FAIL";
        error["message"] = "signature verification failed";
        respond(error, "signature verification failed");
        return;
    }

    // Parse callback body
    Json::CharReaderBuilder builder;
    Json::Value notifyJson;
    std::string parseErrors;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    if (!reader->parse(body.data(), body.data() + body.size(), &notifyJson, &parseErrors))
    {
        Json::Value error;
        error["code"] = "FAIL";
        error["message"] = "invalid json";
        respond(error, "invalid json");
        return;
    }

    // Validate event_type
    const std::string eventType = notifyJson.get("event_type", "").asString();
    if (eventType.empty())
    {
        Json::Value error;
        error["code"] = "FAIL";
        error["message"] = "missing event_type";
        respond(error, "missing event_type");
        return;
    }

    if (eventType.rfind("REFUND.", 0) != 0)
    {
        Json::Value error;
        error["code"] = "FAIL";
        error["message"] = "invalid refund event_type";
        respond(error, "invalid refund event_type");
        return;
    }

    // Validate resource
    if (!notifyJson.isMember("resource"))
    {
        Json::Value error;
        error["code"] = "FAIL";
        error["message"] = "missing resource";
        respond(error, "missing resource");
        return;
    }

    const auto &resource = notifyJson["resource"];
    const std::string resourceType = notifyJson.get("resource_type", "").asString();
    if (resourceType.empty() || resourceType != "encrypt-resource")
    {
        Json::Value error;
        error["code"] = "FAIL";
        error["message"] = "unsupported resource_type";
        respond(error, "unsupported resource_type");
        return;
    }

    const std::string algorithm = resource.get("algorithm", "").asString();
    if (algorithm.empty() || algorithm != "AEAD_AES_256_GCM")
    {
        Json::Value error;
        error["code"] = "FAIL";
        error["message"] = "unsupported resource algorithm";
        respond(error, "unsupported resource algorithm");
        return;
    }

    const std::string ciphertext = resource.get("ciphertext", "").asString();
    const std::string nonceStr = resource.get("nonce", "").asString();
    const std::string associatedData = resource.get("associated_data", "").asString();
    if (ciphertext.empty() || nonceStr.empty())
    {
        Json::Value error;
        error["code"] = "FAIL";
        error["message"] = "invalid resource";
        respond(error, "invalid resource");
        return;
    }

    if (associatedData != "refund")
    {
        Json::Value error;
        error["code"] = "FAIL";
        error["message"] = "invalid refund associated_data";
        respond(error, "invalid refund associated_data");
        return;
    }

    // Decrypt resource
    std::string plaintext;
    std::string decryptError;
    if (!wechatClient_->decryptResource(ciphertext, nonceStr, associatedData, plaintext, decryptError))
    {
        Json::Value error;
        error["code"] = "FAIL";
        error["message"] = decryptError;
        respond(error, decryptError);
        return;
    }

    // Parse decrypted JSON
    Json::Value plainJson;
    Json::CharReaderBuilder plainBuilder;
    std::string plainErrors;
    std::unique_ptr<Json::CharReader> plainReader(plainBuilder.newCharReader());
    if (!plainReader->parse(plaintext.data(), plaintext.data() + plaintext.size(), &plainJson, &plainErrors))
    {
        Json::Value error;
        error["code"] = "FAIL";
        error["message"] = "invalid resource json";
        respond(error, "invalid resource json");
        return;
    }

    // Validate appid and mchid
    const std::string appId = plainJson.get("appid", "").asString();
    const std::string mchId = plainJson.get("mchid", "").asString();
    if (!appId.empty() && wechatClient_ && appId != wechatClient_->getAppId())
    {
        Json::Value error;
        error["code"] = "FAIL";
        error["message"] = "appid mismatch";
        respond(error, "appid mismatch");
        return;
    }
    if (!mchId.empty() && wechatClient_ && mchId != wechatClient_->getMchId())
    {
        Json::Value error;
        error["code"] = "FAIL";
        error["message"] = "mchid mismatch";
        respond(error, "mchid mismatch");
        return;
    }

    // Extract refund details
    const std::string refundNo = plainJson.get("out_refund_no", "").asString();
    const std::string refundStatusRaw = plainJson.get("refund_status", "").asString();
    const std::string refundId = plainJson.get("refund_id", "").asString();

    if (refundNo.empty() || refundStatusRaw.empty())
    {
        Json::Value error;
        error["code"] = "FAIL";
        error["message"] = "missing refund_no/refund_status";
        respond(error, "missing refund_no/refund_status");
        return;
    }

    if (refundStatusRaw == "SUCCESS" && refundId.empty())
    {
        Json::Value error;
        error["code"] = "FAIL";
        error["message"] = "missing refund_id";
        respond(error, "missing refund_id");
        return;
    }

    // Idempotency check
    std::string idempotencyKey = notifyJson.get("id", "").asString();
    if (idempotencyKey.empty())
    {
        idempotencyKey = refundNo + ":" + refundStatusRaw;
    }

    auto cbPtr = std::make_shared<CallbackResult>(std::move(callback));

    auto proceedRefundDb = [this,
                            cbPtr,
                            idempotencyKey,
                            refundNo,
                            refundStatusRaw,
                            refundId,
                            signature,
                            serialNo,
                            plaintext,
                            body,
                            plainJson]() {
        drogon::orm::Mapper<PayIdempotencyModel> idempMapper(dbClient_);
        auto idempCriteria =
            drogon::orm::Criteria(PayIdempotencyModel::Cols::_idempotency_key,
                                  drogon::orm::CompareOperator::EQ,
                                  idempotencyKey);
        idempMapper.findOne(
            idempCriteria,
            [cbPtr](const PayIdempotencyModel &) {
                // Already processed - return success
                Json::Value ok;
                ok["code"] = "SUCCESS";
                ok["message"] = "OK";
                (*cbPtr)(ok, std::error_code());
            },
            [this,
             cbPtr,
             idempotencyKey,
             refundNo,
             refundStatusRaw,
             refundId,
             signature,
             serialNo,
             plaintext,
             body,
             plainJson](const drogon::orm::DrogonDbException &) {
                const std::string requestHash = drogon::utils::getMd5(body);
                PayIdempotencyModel idemp;
                idemp.setIdempotencyKey(idempotencyKey);
                idemp.setRequestHash(requestHash);
                idemp.setResponseSnapshot(plaintext);
                const auto now = trantor::Date::now();
                const auto expiresAt = trantor::Date(
                    now.microSecondsSinceEpoch() +
                    static_cast<int64_t>(7) * 24 * 60 * 60 * 1000000);
                idemp.setExpiresAt(expiresAt);

                drogon::orm::Mapper<PayIdempotencyModel> idempInsert(dbClient_);
                idempInsert.insert(
                    idemp,
                    [this,
                     cbPtr,
                     refundNo,
                     refundStatusRaw,
                     refundId,
                     signature,
                     serialNo,
                     plaintext,
                     body,
                     plainJson](const PayIdempotencyModel &) {
                        const std::string refundStatus =
                            pay::utils::mapRefundStatus(refundStatusRaw);
                        if (refundStatus.empty())
                        {
                            Json::Value error;
                            error["code"] = "FAIL";
                            error["message"] = "invalid refund status";
                            (*cbPtr)(error, std::error_code());
                            return;
                        }

                        drogon::orm::Mapper<PayRefundModel> refundMapper(dbClient_);
                        auto refundCriteria =
                            drogon::orm::Criteria(
                                PayRefundModel::Cols::_refund_no,
                                drogon::orm::CompareOperator::EQ,
                                refundNo);
                        refundMapper.findOne(
                            refundCriteria,
                            [this,
                             cbPtr,
                             refundStatus,
                             refundId,
                             signature,
                             serialNo,
                             refundNo,
                             body,
                             plaintext,
                             plainJson](PayRefundModel refund) {
                                // Already successful - return success
                                if (refund.getValueOfStatus() == "REFUND_SUCCESS")
                                {
                                    Json::Value ok;
                                    ok["code"] = "SUCCESS";
                                    ok["message"] = "OK";
                                    (*cbPtr)(ok, std::error_code());
                                    return;
                                }

                                const auto orderNo = refund.getValueOfOrderNo();
                                const auto paymentNo = refund.getValueOfPaymentNo();
                                const auto refundAmount = refund.getValueOfAmount();
                                const auto &amountJson = plainJson["amount"];
                                const std::string notifyCurrency =
                                    amountJson.get("currency", "").asString();
                                const int64_t notifyRefundFen =
                                    amountJson.get("refund", 0).asInt64();
                                int64_t refundTotalFen = 0;
                                if (!pay::utils::parseAmountToFen(refundAmount,
                                                              refundTotalFen) ||
                                    notifyRefundFen <= 0)
                                {
                                    Json::Value error;
                                    error["code"] = "FAIL";
                                    error["message"] = "invalid refund amount in callback";
                                    (*cbPtr)(error, std::error_code());
                                    return;
                                }
                                if (notifyRefundFen != refundTotalFen)
                                {
                                    Json::Value error;
                                    error["code"] = "FAIL";
                                    error["message"] = "refund amount mismatch";
                                    (*cbPtr)(error, std::error_code());
                                    return;
                                }

                                drogon::orm::Mapper<PayOrderModel> orderMapper(dbClient_);
                                auto orderCriteria =
                                    drogon::orm::Criteria(
                                        PayOrderModel::Cols::_order_no,
                                        drogon::orm::CompareOperator::EQ,
                                        orderNo);
                                orderMapper.findOne(
                                orderCriteria,
                                [this,
                                 cbPtr,
                                 refundStatus,
                                 refundId,
                                 refundAmount,
                                 orderNo,
                                 paymentNo,
                                 notifyCurrency,
                                 signature,
                                 serialNo,
                                 refundNo,
                                 body,
                                 plaintext,
                                 refund](const PayOrderModel &order) mutable {
                                    const std::string orderCurrency =
                                        order.getValueOfCurrency();
                                    if (!notifyCurrency.empty() &&
                                        notifyCurrency != orderCurrency)
                                    {
                                        Json::Value error;
                                        error["code"] = "FAIL";
                                        error["message"] = "refund currency mismatch";
                                        (*cbPtr)(error, std::error_code());
                                        return;
                                    }

                                    dbClient_->newTransactionAsync(
                                        [this,
                                         cbPtr,
                                         refundStatus,
                                         refundAmount,
                                         orderNo,
                                         paymentNo,
                                         order,
                                         signature,
                                         serialNo,
                                         body,
                                         refundNo,
                                         plaintext,
                                         refund](const std::shared_ptr<drogon::orm::Transaction> &transPtr) mutable {
                                            auto respondDbError =
                                                [cbPtr, transPtr](const drogon::orm::DrogonDbException &e) {
                                                    transPtr->rollback();
                                                    Json::Value error;
                                                    error["code"] = "FAIL";
                                                    error["message"] = std::string("db error: ") + e.base().what();
                                                    (*cbPtr)(error, std::error_code());
                                                };

                                            drogon::orm::Mapper<PayRefundModel>
                                                refundUpdater(transPtr);
                                            refundUpdater.update(
                                                refund,
                                                [this,
                                                 cbPtr,
                                                 refundStatus,
                                                 refundAmount,
                                                 orderNo,
                                                 paymentNo,
                                                 order,
                                                 signature,
                                                 serialNo,
                                                 body,
                                                 refundNo,
                                                 plaintext,
                                                 transPtr](const size_t) {
                                                    transPtr->execSqlAsync(
                                                        "UPDATE pay_refund "
                                                        "SET response_payload = $1 "
                                                        "WHERE refund_no = $2",
                                                        [](const drogon::orm::Result &) {},
                                                        [cbPtr, transPtr](const drogon::orm::DrogonDbException &e) {
                                                            transPtr->rollback();
                                                            Json::Value error;
                                                            error["code"] = "FAIL";
                                                            error["message"] = std::string("db error: ") + e.base().what();
                                                            (*cbPtr)(error, std::error_code());
                                                        },
                                                        plaintext,
                                                        refundNo);
                                                    if (refundStatus == "REFUND_SUCCESS")
                                                    {
                                                        auto transDb =
                                                            std::static_pointer_cast<
                                                                drogon::orm::DbClient>(transPtr);
                                                        insertLedgerEntry(
                                                            transDb,
                                                            order.getValueOfUserId(),
                                                            orderNo,
                                                            paymentNo,
                                                            "REFUND",
                                                            refundAmount);
                                                    }
                                                    PayCallbackModel callbackRow;
                                                    callbackRow.setPaymentNo(
                                                        paymentNo);
                                                    callbackRow.setRawBody(
                                                        body);
                                                    callbackRow.setSignature(
                                                        signature);
                                                    callbackRow.setSerialNo(
                                                        serialNo);
                                                    callbackRow.setVerified(true);
                                                    callbackRow.setProcessed(true);
                                                    callbackRow.setReceivedAt(
                                                        trantor::Date::now());

                                                    drogon::orm::Mapper<PayCallbackModel>
                                                        callbackMapper(transPtr);
                                                    callbackMapper.insert(
                                                        callbackRow,
                                                        [cbPtr, transPtr](const PayCallbackModel &) {
                                                            Json::Value ok;
                                                            ok["code"] = "SUCCESS";
                                                            ok["message"] = "OK";
                                                            (*cbPtr)(ok, std::error_code());
                                                        },
                                                        [cbPtr, transPtr](
                                                            const drogon::orm::
                                                                DrogonDbException &e) {
                                                            transPtr->rollback();
                                                            Json::Value error;
                                                            error["code"] = "FAIL";
                                                            error["message"] = std::string("db error: ") + e.base().what();
                                                            (*cbPtr)(error, std::error_code());
                                                        });
                                                },
                                                respondDbError);
                                        });
                                },
                                [cbPtr](const drogon::orm::DrogonDbException &e) {
                                    Json::Value error;
                                    error["code"] = "FAIL";
                                    error["message"] = std::string("order not found: ") + e.base().what();
                                    (*cbPtr)(error, std::error_code());
                                });
                        },
                        [cbPtr](const drogon::orm::DrogonDbException &e) {
                            Json::Value error;
                            error["code"] = "FAIL";
                            error["message"] = std::string("refund not found: ") + e.base().what();
                            (*cbPtr)(error, std::error_code());
                        });
                        },
                        [cbPtr](const drogon::orm::DrogonDbException &e) {
                            Json::Value error;
                            error["code"] = "FAIL";
                            error["message"] = std::string("db error: ") + e.base().what();
                            (*cbPtr)(error, std::error_code());
                        });
                });
    };

    // Skip Redis idempotency for now (simpler implementation)
    proceedRefundDb();
}

bool CallbackService::verifySignature(
    const std::string& body,
    const std::string& signature,
    const std::string& timestamp,
    const std::string& nonce,
    const std::string& serialNo) {

    if (!wechatClient_)
    {
        LOG_ERROR << "WechatPayClient is null";
        return false;
    }

    std::string verifyError;
    if (!wechatClient_->verifyCallback(timestamp, nonce, body, signature, serialNo, verifyError))
    {
        LOG_WARN << "Signature verification failed: " << verifyError;
        return false;
    }

    return true;
}
