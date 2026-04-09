#include <drogon/drogon.h>
#include <drogon/drogon_test.h>
#include <drogon/nosql/RedisClient.h>
#include <drogon/utils/Utilities.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include "../models/PayOrder.h"
#include "../models/PayPayment.h"
#include "../models/PayRefund.h"
#include "../plugins/PayPlugin.h"
#include "../utils/PayUtils.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <thread>

namespace
{
bool loadConfig(Json::Value &root)
{
    const auto cwd = std::filesystem::current_path();
    const std::vector<std::filesystem::path> candidates = {
        cwd / "config.json",
        cwd / "test" / "Release" / "config.json",
        cwd / "Release" / "config.json",
        cwd.parent_path() / "config.json",
        cwd.parent_path() / "test" / "Release" / "config.json",
        cwd.parent_path() / "Release" / "config.json"};

    std::filesystem::path configPath;
    for (const auto &candidate : candidates)
    {
        if (std::filesystem::exists(candidate))
        {
            configPath = candidate;
            break;
        }
    }

    if (configPath.empty())
    {
        return false;
    }

    std::ifstream in(configPath.string());
    if (!in)
    {
        return false;
    }

    Json::CharReaderBuilder builder;
    std::string errors;
    const bool ok = Json::parseFromStream(builder, in, &root, &errors);
    return ok;
}

std::string buildPgConnInfo(const Json::Value &db)
{
    const std::string host = db.get("host", "").asString();
    const int port = db.get("port", 5432).asInt();
    const std::string dbname = db.get("dbname", "").asString();
    const std::string user = db.get("user", "").asString();
    const std::string passwd = db.get("passwd", "").asString();

    std::string connInfo = "host=" + host + " port=" + std::to_string(port) +
                           " dbname=" + dbname + " user=" + user;
    if (!passwd.empty())
    {
        connInfo += " password=" + passwd;
    }
    return connInfo;
}

drogon::nosql::RedisClientPtr buildRedisClient(const Json::Value &redis)
{
    const std::string host = redis.get("host", "127.0.0.1").asString();
    const int port = redis.get("port", 6379).asInt();
    const std::string password = redis.get("passwd", "").asString();
    const unsigned int db = redis.get("db", 0).asUInt();
    const std::string username = redis.get("username", "").asString();

    trantor::InetAddress addr(host, static_cast<uint16_t>(port));
    return drogon::nosql::RedisClient::newRedisClient(
        addr, 1, password, db, username);
}

bool pingRedis(const drogon::nosql::RedisClientPtr &client)
{
    if (!client)
    {
        return false;
    }

    auto pingPromise = std::make_shared<std::promise<bool>>();
    auto pingFuture = pingPromise->get_future();
    client->execCommandAsync(
        [pingPromise](const drogon::nosql::RedisResult &r) {
            try
            {
                pingPromise->set_value(r.asString() == "PONG");
            }
            catch (...)
            {
                pingPromise->set_value(false);
            }
        },
        [pingPromise](const drogon::nosql::RedisException &) {
            pingPromise->set_value(false);
        },
        "PING");

    if (pingFuture.wait_for(std::chrono::seconds(2)) !=
        std::future_status::ready)
    {
        return false;
    }
    return pingFuture.get();
}

bool writeTempPrivateKey(const std::filesystem::path &path)
{
    EVP_PKEY *pkey = EVP_PKEY_new();
    if (!pkey)
    {
        return false;
    }

    RSA *rsa = RSA_new();
    BIGNUM *bn = BN_new();
    if (!rsa || !bn)
    {
        if (bn)
        {
            BN_free(bn);
        }
        if (rsa)
        {
            RSA_free(rsa);
        }
        EVP_PKEY_free(pkey);
        return false;
    }

    if (BN_set_word(bn, RSA_F4) != 1 ||
        RSA_generate_key_ex(rsa, 2048, bn, nullptr) != 1)
    {
        BN_free(bn);
        RSA_free(rsa);
        EVP_PKEY_free(pkey);
        return false;
    }

    if (EVP_PKEY_assign_RSA(pkey, rsa) != 1)
    {
        BN_free(bn);
        RSA_free(rsa);
        EVP_PKEY_free(pkey);
        return false;
    }
    BN_free(bn);

    std::ofstream out(path.string(), std::ios::binary);
    if (!out)
    {
        EVP_PKEY_free(pkey);
        return false;
    }

    BIO *bio = BIO_new(BIO_s_mem());
    if (!bio)
    {
        EVP_PKEY_free(pkey);
        return false;
    }
    if (PEM_write_bio_PrivateKey(bio, pkey, nullptr, nullptr, 0, nullptr,
                                 nullptr) != 1)
    {
        BIO_free(bio);
        EVP_PKEY_free(pkey);
        return false;
    }

    BUF_MEM *buf = nullptr;
    BIO_get_mem_ptr(bio, &buf);
    if (!buf || !buf->data || buf->length == 0)
    {
        BIO_free(bio);
        EVP_PKEY_free(pkey);
        return false;
    }

    out.write(buf->data, static_cast<std::streamsize>(buf->length));
    BIO_free(bio);
    EVP_PKEY_free(pkey);
    return static_cast<bool>(out);
}
}  // namespace

DROGON_TEST(PayPlugin_QueryRefund_NoWechatClient)
{
    Json::Value root;
    CHECK(loadConfig(root));
    CHECK(root.isMember("db_clients"));
    CHECK(root["db_clients"].isArray());
    CHECK(!root["db_clients"].empty());

    const auto &db = root["db_clients"][0];
    const std::string connInfo = buildPgConnInfo(db);
    CHECK(!connInfo.empty());

    auto client = drogon::orm::DbClient::newPgClient(connInfo, 1);
    CHECK(client != nullptr);

    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_payment ("
        "id BIGSERIAL PRIMARY KEY,"
        "order_no VARCHAR(64) NOT NULL,"
        "payment_no VARCHAR(64) NOT NULL UNIQUE,"
        "channel_trade_no VARCHAR(64),"
        "status VARCHAR(24) NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "request_payload TEXT,"
        "response_payload TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");
    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_refund ("
        "id BIGSERIAL PRIMARY KEY,"
        "refund_no VARCHAR(64) NOT NULL UNIQUE,"
        "order_no VARCHAR(64) NOT NULL,"
        "payment_no VARCHAR(64) NOT NULL,"
        "channel_refund_no VARCHAR(64),"
        "status VARCHAR(24) NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "response_payload TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");
    client->execSqlSync(
        "ALTER TABLE pay_refund "
        "ADD COLUMN IF NOT EXISTS response_payload TEXT");

    const std::string refundNo = "refund_" + drogon::utils::getUuid();
    const std::string orderNo = "ord_" + drogon::utils::getUuid();
    const std::string paymentNo = "pay_" + drogon::utils::getUuid();
    const std::string amount = "9.99";

    using PayRefund = drogon_model::pay_test::PayRefund;
    drogon::orm::Mapper<PayRefund> refundMapper(client);
    PayRefund refund;
    refund.setRefundNo(refundNo);
    refund.setOrderNo(orderNo);
    refund.setPaymentNo(paymentNo);
    refund.setStatus("REFUNDING");
    refund.setAmount(amount);
    refund.setCreatedAt(trantor::Date::now());
    refund.setUpdatedAt(trantor::Date::now());
    refundMapper.insert(refund);

    PayPlugin plugin;
    plugin.setTestClients(nullptr, client);

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setParameter("refund_no", refundNo);

    std::promise<drogon::HttpResponsePtr> promise;
    plugin.queryRefund(
        req,
        [&promise](const drogon::HttpResponsePtr &resp) {
            promise.set_value(resp);
        });

    auto future = promise.get_future();
    if (future.wait_for(std::chrono::seconds(5)) !=
        std::future_status::ready)
    {
        return;
    }
    const auto resp = future.get();
    CHECK(resp != nullptr);
    CHECK(resp->statusCode() == drogon::k200OK);
    const auto respJson = resp->getJsonObject();
    CHECK(respJson != nullptr);
    CHECK((*respJson)["refund_no"].asString() == refundNo);
    CHECK((*respJson)["order_no"].asString() == orderNo);
    CHECK((*respJson)["payment_no"].asString() == paymentNo);
    CHECK((*respJson)["status"].asString() == "REFUNDING");
    CHECK((*respJson)["amount"].asString() == amount);

    client->execSqlSync("DELETE FROM pay_refund WHERE refund_no = $1",
                        refundNo);
}

DROGON_TEST(PayPlugin_QueryRefund_WechatQueryError)
{
    Json::Value root;
    CHECK(loadConfig(root));
    CHECK(root.isMember("db_clients"));
    CHECK(root["db_clients"].isArray());
    CHECK(!root["db_clients"].empty());

    const auto &db = root["db_clients"][0];
    const std::string connInfo = buildPgConnInfo(db);
    CHECK(!connInfo.empty());

    auto client = drogon::orm::DbClient::newPgClient(connInfo, 1);
    CHECK(client != nullptr);

    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_refund ("
        "id BIGSERIAL PRIMARY KEY,"
        "refund_no VARCHAR(64) NOT NULL UNIQUE,"
        "order_no VARCHAR(64) NOT NULL,"
        "payment_no VARCHAR(64) NOT NULL,"
        "channel_refund_no VARCHAR(64),"
        "status VARCHAR(24) NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "response_payload TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");

    const std::string refundNo = "refund_" + drogon::utils::getUuid();
    const std::string orderNo = "ord_" + drogon::utils::getUuid();
    const std::string paymentNo = "pay_" + drogon::utils::getUuid();
    const std::string amount = "19.99";

    using PayRefund = drogon_model::pay_test::PayRefund;
    drogon::orm::Mapper<PayRefund> refundMapper(client);
    PayRefund refund;
    refund.setRefundNo(refundNo);
    refund.setOrderNo(orderNo);
    refund.setPaymentNo(paymentNo);
    refund.setStatus("REFUNDING");
    refund.setAmount(amount);
    refund.setCreatedAt(trantor::Date::now());
    refund.setUpdatedAt(trantor::Date::now());
    refundMapper.insert(refund);

    Json::Value wechatConfig;
    wechatConfig["api_v3_key"] = "0123456789abcdef0123456789abcdef";
    wechatConfig["app_id"] = "wx_app";
    wechatConfig["mch_id"] = "";
    wechatConfig["notify_url"] = "https://notify.invalid";
    auto wechatClient = std::make_shared<WechatPayClient>(wechatConfig);

    PayPlugin plugin;
    plugin.setTestClients(wechatClient, client);

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setParameter("refund_no", refundNo);

    std::promise<drogon::HttpResponsePtr> promise;
    plugin.queryRefund(
        req,
        [&promise](const drogon::HttpResponsePtr &resp) {
            promise.set_value(resp);
        });

    auto future = promise.get_future();
    CHECK(future.wait_for(std::chrono::seconds(5)) ==
          std::future_status::ready);
    const auto resp = future.get();
    CHECK(resp != nullptr);
    CHECK(resp->statusCode() == drogon::k200OK);
    CHECK(resp->getHeader("X-Wechat-Query-Error") ==
          "wechat pay config missing mch_id/serial_no/private_key_path");
    const auto respJson = resp->getJsonObject();
    CHECK(respJson != nullptr);
    CHECK((*respJson)["refund_no"].asString() == refundNo);
    CHECK((*respJson)["status"].asString() == "REFUNDING");
    CHECK((*respJson)["updated_at"].isString());

    client->execSqlSync("DELETE FROM pay_refund WHERE refund_no = $1",
                        refundNo);
}

DROGON_TEST(PayPlugin_Refund_IdempotencyConflict)
{
    Json::Value root;
    CHECK(loadConfig(root));
    CHECK(root.isMember("db_clients"));
    CHECK(root["db_clients"].isArray());
    CHECK(!root["db_clients"].empty());

    const auto &db = root["db_clients"][0];
    const std::string connInfo = buildPgConnInfo(db);
    CHECK(!connInfo.empty());

    auto client = drogon::orm::DbClient::newPgClient(connInfo, 1);
    CHECK(client != nullptr);

    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_idempotency ("
        "idempotency_key VARCHAR(64) PRIMARY KEY,"
        "request_hash TEXT NOT NULL,"
        "response_snapshot TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "expires_at TIMESTAMPTZ NOT NULL)");

    const std::string idempotencyKey = "idem_" + drogon::utils::getUuid();
    Json::Value payload;
    payload["order_no"] = "ord_" + drogon::utils::getUuid();
    payload["amount"] = "9.99";
    const std::string body = pay::utils::toJsonString(payload);

    const std::string idempKey = "refund:" + idempotencyKey;
    client->execSqlSync(
        "INSERT INTO pay_idempotency "
        "(idempotency_key, request_hash, response_snapshot, expires_at) "
        "VALUES ($1, $2, $3, NOW() + INTERVAL '1 day')",
        idempKey,
        "other_hash",
        "{}");

    PayPlugin plugin;
    plugin.setTestClients(nullptr, client);

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    req->setBody(body);
    req->addHeader("Idempotency-Key", idempotencyKey);

    std::promise<drogon::HttpResponsePtr> promise;
    plugin.refund(
        req,
        [&promise](const drogon::HttpResponsePtr &resp) {
            promise.set_value(resp);
        });

    auto future = promise.get_future();
    CHECK(future.wait_for(std::chrono::seconds(5)) ==
          std::future_status::ready);
    const auto resp = future.get();
    CHECK(resp != nullptr);
    CHECK(resp->statusCode() == drogon::k409Conflict);
    CHECK(resp->body().find("idempotency key conflict") != std::string::npos);

    client->execSqlSync("DELETE FROM pay_idempotency WHERE idempotency_key = $1",
                        idempKey);
}

DROGON_TEST(PayPlugin_Refund_IdempotencySnapshot)
{
    Json::Value root;
    CHECK(loadConfig(root));
    CHECK(root.isMember("db_clients"));
    CHECK(root["db_clients"].isArray());
    CHECK(!root["db_clients"].empty());

    const auto &db = root["db_clients"][0];
    const std::string connInfo = buildPgConnInfo(db);
    CHECK(!connInfo.empty());

    auto client = drogon::orm::DbClient::newPgClient(connInfo, 1);
    CHECK(client != nullptr);

    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_idempotency ("
        "idempotency_key VARCHAR(64) PRIMARY KEY,"
        "request_hash TEXT NOT NULL,"
        "response_snapshot TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "expires_at TIMESTAMPTZ NOT NULL)");

    const std::string idempotencyKey = "idem_" + drogon::utils::getUuid();
    Json::Value payload;
    payload["order_no"] = "ord_" + drogon::utils::getUuid();
    payload["amount"] = "12.34";
    const std::string body = pay::utils::toJsonString(payload);
    const std::string requestHash = drogon::utils::getSha256(body);

    Json::Value snapshot;
    snapshot["refund_no"] = "refund_prev";
    snapshot["order_no"] = payload["order_no"];
    snapshot["status"] = "REFUNDING";
    const std::string snapshotBody = pay::utils::toJsonString(snapshot);

    const std::string idempKey = "refund:" + idempotencyKey;
    client->execSqlSync(
        "INSERT INTO pay_idempotency "
        "(idempotency_key, request_hash, response_snapshot, expires_at) "
        "VALUES ($1, $2, $3, NOW() + INTERVAL '1 day')",
        idempKey,
        requestHash,
        snapshotBody);

    PayPlugin plugin;
    plugin.setTestClients(nullptr, client);

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    req->setBody(body);
    req->addHeader("Idempotency-Key", idempotencyKey);

    std::promise<drogon::HttpResponsePtr> promise;
    plugin.refund(
        req,
        [&promise](const drogon::HttpResponsePtr &resp) {
            promise.set_value(resp);
        });

    auto future = promise.get_future();
    CHECK(future.wait_for(std::chrono::seconds(5)) ==
          std::future_status::ready);
    const auto resp = future.get();
    CHECK(resp != nullptr);
    CHECK(resp->statusCode() == drogon::k200OK);
    auto respJson = resp->getJsonObject();
    CHECK(respJson != nullptr);
    CHECK((*respJson)["refund_no"].asString() == "refund_prev");
    CHECK((*respJson)["status"].asString() == "REFUNDING");

    client->execSqlSync("DELETE FROM pay_idempotency WHERE idempotency_key = $1",
                        idempKey);
}

DROGON_TEST(PayPlugin_Refund_IdempotencyInProgress)
{
    Json::Value root;
    CHECK(loadConfig(root));
    CHECK(root.isMember("db_clients"));
    CHECK(root["db_clients"].isArray());
    CHECK(!root["db_clients"].empty());
    CHECK(root.isMember("redis_clients"));
    CHECK(root["redis_clients"].isArray());
    CHECK(!root["redis_clients"].empty());

    const auto &db = root["db_clients"][0];
    const std::string connInfo = buildPgConnInfo(db);
    CHECK(!connInfo.empty());

    auto client = drogon::orm::DbClient::newPgClient(connInfo, 1);
    CHECK(client != nullptr);

    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_idempotency ("
        "idempotency_key VARCHAR(64) PRIMARY KEY,"
        "request_hash TEXT NOT NULL,"
        "response_snapshot TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "expires_at TIMESTAMPTZ NOT NULL)");

    auto redisClient = buildRedisClient(root["redis_clients"][0]);
    CHECK(redisClient != nullptr);
    if (!pingRedis(redisClient))
    {
        return;
    }

    const std::string idempotencyKey = "idem_" + drogon::utils::getUuid();
    Json::Value payload;
    payload["order_no"] = "ord_" + drogon::utils::getUuid();
    payload["amount"] = "1.23";
    const std::string body = pay::utils::toJsonString(payload);
    const std::string requestHash = drogon::utils::getSha256(body);

    const std::string redisKey = "pay:idempotency:refund:" + idempotencyKey;
    const auto setResult = redisClient->execCommandSync<std::string>(
        [](const drogon::nosql::RedisResult &r) {
            if (r.type() == drogon::nosql::RedisResultType::kNil)
            {
                return std::string("NIL");
            }
            return r.asString();
        },
        "SET %s %s NX EX %d",
        redisKey.c_str(),
        requestHash.c_str(),
        60);
    CHECK(setResult == "OK");

    PayPlugin plugin;
    plugin.setTestClients(nullptr, client, redisClient, true);

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    req->setBody(body);
    req->addHeader("Idempotency-Key", idempotencyKey);

    std::promise<drogon::HttpResponsePtr> promise;
    plugin.refund(
        req,
        [&promise](const drogon::HttpResponsePtr &resp) {
            promise.set_value(resp);
        });

    auto future = promise.get_future();
    CHECK(future.wait_for(std::chrono::seconds(5)) ==
          std::future_status::ready);
    const auto resp = future.get();
    CHECK(resp != nullptr);
    CHECK(resp->statusCode() == drogon::k409Conflict);
    CHECK(resp->body().find("idempotency key in progress") != std::string::npos);

    redisClient->execCommandSync<int>(
        [](const drogon::nosql::RedisResult &r) {
            return static_cast<int>(r.asInteger());
        },
        "DEL %s",
        redisKey.c_str());
    client->execSqlSync("DELETE FROM pay_idempotency WHERE idempotency_key = $1",
                        "refund:" + idempotencyKey);
}

DROGON_TEST(PayPlugin_Refund_WechatPayloadExtras)
{
    Json::Value root;
    CHECK(loadConfig(root));
    CHECK(root.isMember("db_clients"));
    CHECK(root["db_clients"].isArray());
    CHECK(!root["db_clients"].empty());

    const auto &db = root["db_clients"][0];
    const std::string connInfo = buildPgConnInfo(db);
    CHECK(!connInfo.empty());

    auto client = drogon::orm::DbClient::newPgClient(connInfo, 1);
    CHECK(client != nullptr);

    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_order ("
        "id BIGSERIAL PRIMARY KEY,"
        "order_no VARCHAR(64) NOT NULL UNIQUE,"
        "user_id BIGINT NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "currency VARCHAR(16) NOT NULL,"
        "status VARCHAR(24) NOT NULL,"
        "channel VARCHAR(16) NOT NULL,"
        "title VARCHAR(128) NOT NULL,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");
    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_payment ("
        "id BIGSERIAL PRIMARY KEY,"
        "order_no VARCHAR(64) NOT NULL,"
        "payment_no VARCHAR(64) NOT NULL UNIQUE,"
        "channel_trade_no VARCHAR(64),"
        "status VARCHAR(24) NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "request_payload TEXT,"
        "response_payload TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");
    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_refund ("
        "id BIGSERIAL PRIMARY KEY,"
        "refund_no VARCHAR(64) NOT NULL UNIQUE,"
        "order_no VARCHAR(64) NOT NULL,"
        "payment_no VARCHAR(64) NOT NULL,"
        "channel_refund_no VARCHAR(64),"
        "status VARCHAR(24) NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "response_payload TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");

    const std::string orderNo = "ord_" + drogon::utils::getUuid();
    const std::string paymentNo = "pay_" + drogon::utils::getUuid();
    const std::string amount = "8.88";

    using PayOrder = drogon_model::pay_test::PayOrder;
    drogon::orm::Mapper<PayOrder> orderMapper(client);
    PayOrder order;
    order.setOrderNo(orderNo);
    order.setUserId(30002);
    order.setAmount(amount);
    order.setCurrency("CNY");
    order.setStatus("PAID");
    order.setChannel("wechat");
    order.setTitle("Refund Payload");
    order.setCreatedAt(trantor::Date::now());
    order.setUpdatedAt(trantor::Date::now());
    orderMapper.insert(order);

    using PayPayment = drogon_model::pay_test::PayPayment;
    drogon::orm::Mapper<PayPayment> paymentMapper(client);
    PayPayment payment;
    payment.setOrderNo(orderNo);
    payment.setPaymentNo(paymentNo);
    payment.setStatus("SUCCESS");
    payment.setAmount(amount);
    payment.setCreatedAt(trantor::Date::now());
    payment.setUpdatedAt(trantor::Date::now());
    paymentMapper.insert(payment);
    const auto paymentRows = client->execSqlSync(
        "SELECT COUNT(*) AS cnt FROM pay_payment WHERE payment_no = $1",
        paymentNo);
    CHECK(!paymentRows.empty());
    CHECK(paymentRows.front()["cnt"].as<int64_t>() == 1);

    const uint16_t port = 24089;
    std::atomic<bool> sawReason(false);
    std::atomic<bool> sawNotify(false);
    std::atomic<bool> sawFunds(false);
    drogon::app().addListener("127.0.0.1", port);
    drogon::app().registerHandler(
        "/v3/refund/domestic/refunds",
        [&sawReason, &sawNotify, &sawFunds](const drogon::HttpRequestPtr &req,
           std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
            Json::CharReaderBuilder builder;
            std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
            Json::Value json;
            std::string errors;
            const auto rawBody = std::string(req->body());
            if (reader->parse(rawBody.data(),
                              rawBody.data() + rawBody.size(),
                              &json,
                              &errors))
            {
                if (json.get("reason", "").asString() == "Test reason")
                {
                    sawReason.store(true);
                }
                if (json.get("notify_url", "").asString() ==
                    "https://notify.refund")
                {
                    sawNotify.store(true);
                }
                if (json.get("funds_account", "").asString() ==
                    "AVAILABLE")
                {
                    sawFunds.store(true);
                }
            }
            Json::Value respBody;
            respBody["status"] = "SUCCESS";
            respBody["refund_id"] = "wx_refund_payload";
            auto resp = drogon::HttpResponse::newHttpJsonResponse(respBody);
            cb(resp);
        },
        {drogon::Post});

    std::thread serverThread([]() { drogon::app().run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    const auto tempDir = std::filesystem::temp_directory_path();
    const auto keyPath =
        tempDir / ("wechatpay_key_" + drogon::utils::getUuid() + ".pem");
    CHECK(writeTempPrivateKey(keyPath));

    Json::Value wechatConfig;
    wechatConfig["api_base"] =
        "http://127.0.0.1:" + std::to_string(port);
    wechatConfig["mch_id"] = "mch_123";
    wechatConfig["serial_no"] = "SERIAL_REFUND_EX";
    wechatConfig["private_key_path"] = keyPath.string();
    wechatConfig["api_v3_key"] = "0123456789abcdef0123456789abcdef";
    wechatConfig["notify_url"] = "https://notify.invalid";
    auto wechatClient = std::make_shared<WechatPayClient>(wechatConfig);

    PayPlugin plugin;
    plugin.setTestClients(wechatClient, client);

    Json::Value payload;
    payload["order_no"] = orderNo;
    payload["payment_no"] = paymentNo;
    payload["amount"] = amount;
    payload["reason"] = "Test reason";
    payload["notify_url"] = "https://notify.refund";
    payload["funds_account"] = "AVAILABLE";
    const std::string body = pay::utils::toJsonString(payload);

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    req->setBody(body);

    std::promise<drogon::HttpResponsePtr> promise;
    plugin.refund(
        req,
        [&promise](const drogon::HttpResponsePtr &resp) {
            promise.set_value(resp);
        });

    auto future = promise.get_future();
    CHECK(future.wait_for(std::chrono::seconds(5)) ==
          std::future_status::ready);
    const auto resp = future.get();
    CHECK(resp != nullptr);
    CHECK(resp->statusCode() == drogon::k200OK);
    const auto respJson = resp->getJsonObject();
    CHECK(respJson != nullptr);
    CHECK((*respJson)["payment_no"].asString() == paymentNo);
    CHECK((*respJson)["amount"].asString() == amount);
    CHECK((*respJson)["status"].asString() == "REFUND_SUCCESS");
    const auto refundNo = (*respJson)["refund_no"].asString();
    CHECK(!refundNo.empty());
    CHECK(sawReason.load());
    CHECK(sawNotify.load());
    CHECK(sawFunds.load());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const auto refundRows = client->execSqlSync(
        "SELECT response_payload FROM pay_refund WHERE refund_no = $1",
        refundNo);
    CHECK(!refundRows.empty());
    CHECK(!refundRows.front()["response_payload"].isNull());
    const auto payloadText =
        refundRows.front()["response_payload"].as<std::string>();
    CHECK(payloadText.find("\"refund_id\":\"wx_refund_payload\"") !=
          std::string::npos);
    CHECK(payloadText.find("\"status\":\"SUCCESS\"") != std::string::npos);

    drogon::app().quit();
    if (serverThread.joinable())
    {
        serverThread.join();
    }

    std::error_code ec;
    std::filesystem::remove(keyPath, ec);
    client->execSqlSync("DELETE FROM pay_refund WHERE order_no = $1",
                        orderNo);
    client->execSqlSync("DELETE FROM pay_payment WHERE payment_no = $1",
                        paymentNo);
    client->execSqlSync("DELETE FROM pay_order WHERE order_no = $1", orderNo);
}

DROGON_TEST(PayPlugin_Refund_WechatErrorPersistsPayload)
{
    Json::Value root;
    CHECK(loadConfig(root));
    CHECK(root.isMember("db_clients"));
    CHECK(root["db_clients"].isArray());
    CHECK(!root["db_clients"].empty());

    const auto &db = root["db_clients"][0];
    const std::string connInfo = buildPgConnInfo(db);
    CHECK(!connInfo.empty());

    auto client = drogon::orm::DbClient::newPgClient(connInfo, 1);
    CHECK(client != nullptr);

    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_order ("
        "id BIGSERIAL PRIMARY KEY,"
        "order_no VARCHAR(64) NOT NULL UNIQUE,"
        "user_id BIGINT NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "currency VARCHAR(16) NOT NULL,"
        "status VARCHAR(24) NOT NULL,"
        "channel VARCHAR(16) NOT NULL,"
        "title VARCHAR(128) NOT NULL,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");
    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_payment ("
        "id BIGSERIAL PRIMARY KEY,"
        "order_no VARCHAR(64) NOT NULL,"
        "payment_no VARCHAR(64) NOT NULL UNIQUE,"
        "channel_trade_no VARCHAR(64),"
        "status VARCHAR(24) NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "request_payload TEXT,"
        "response_payload TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");
    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_refund ("
        "id BIGSERIAL PRIMARY KEY,"
        "refund_no VARCHAR(64) NOT NULL UNIQUE,"
        "order_no VARCHAR(64) NOT NULL,"
        "payment_no VARCHAR(64) NOT NULL,"
        "channel_refund_no VARCHAR(64),"
        "status VARCHAR(24) NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "response_payload TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");

    const std::string orderNo = "ord_" + drogon::utils::getUuid();
    const std::string paymentNo = "pay_" + drogon::utils::getUuid();
    const std::string amount = "9.01";

    using PayOrder = drogon_model::pay_test::PayOrder;
    drogon::orm::Mapper<PayOrder> orderMapper(client);
    PayOrder order;
    order.setOrderNo(orderNo);
    order.setUserId(30009);
    order.setAmount(amount);
    order.setCurrency("CNY");
    order.setStatus("PAID");
    order.setChannel("wechat");
    order.setTitle("Refund Wechat Error");
    order.setCreatedAt(trantor::Date::now());
    order.setUpdatedAt(trantor::Date::now());
    orderMapper.insert(order);

    using PayPayment = drogon_model::pay_test::PayPayment;
    drogon::orm::Mapper<PayPayment> paymentMapper(client);
    PayPayment payment;
    payment.setOrderNo(orderNo);
    payment.setPaymentNo(paymentNo);
    payment.setStatus("SUCCESS");
    payment.setAmount(amount);
    payment.setCreatedAt(trantor::Date::now());
    payment.setUpdatedAt(trantor::Date::now());
    paymentMapper.insert(payment);

    Json::Value wechatConfig;
    wechatConfig["api_base"] = "https://api.mch.weixin.qq.com";
    wechatConfig["mch_id"] = "";
    wechatConfig["serial_no"] = "";
    wechatConfig["private_key_path"] = "";
    wechatConfig["api_v3_key"] = "0123456789abcdef0123456789abcdef";
    auto wechatClient = std::make_shared<WechatPayClient>(wechatConfig);

    PayPlugin plugin;
    plugin.setTestClients(wechatClient, client);

    Json::Value payload;
    payload["order_no"] = orderNo;
    payload["payment_no"] = paymentNo;
    payload["amount"] = amount;
    const std::string body = pay::utils::toJsonString(payload);

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    req->setBody(body);

    std::promise<drogon::HttpResponsePtr> promise;
    plugin.refund(
        req,
        [&promise](const drogon::HttpResponsePtr &resp) {
            promise.set_value(resp);
        });

    auto future = promise.get_future();
    CHECK(future.wait_for(std::chrono::seconds(5)) ==
          std::future_status::ready);
    const auto resp = future.get();
    CHECK(resp != nullptr);
    CHECK(resp->statusCode() == drogon::k502BadGateway);
    const auto respJson = resp->getJsonObject();
    CHECK(respJson != nullptr);
    CHECK((*respJson)["order_no"].asString() == orderNo);
    CHECK((*respJson)["payment_no"].asString() == paymentNo);
    CHECK((*respJson)["amount"].asString() == amount);
    CHECK((*respJson)["status"].asString() == "REFUND_FAIL");
    CHECK((*respJson)["error"].asString().find("wechat pay config missing") !=
          std::string::npos);
    CHECK((*respJson)["wechat_response"]["error"].asString().find(
              "wechat pay config missing") != std::string::npos);

    std::string refundStatus;
    std::string responsePayload;
    for (int i = 0; i < 20; ++i)
    {
        const auto rows = client->execSqlSync(
            "SELECT status, response_payload FROM pay_refund "
            "WHERE order_no = $1 AND payment_no = $2 "
            "ORDER BY created_at DESC LIMIT 1",
            orderNo,
            paymentNo);
        if (!rows.empty())
        {
            refundStatus = rows.front()["status"].as<std::string>();
            if (!rows.front()["response_payload"].isNull())
            {
                responsePayload =
                    rows.front()["response_payload"].as<std::string>();
            }
            if (refundStatus == "REFUND_FAIL" && !responsePayload.empty())
            {
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    CHECK(refundStatus == "REFUND_FAIL");
    CHECK(responsePayload.find("wechat pay config missing") !=
          std::string::npos);

    client->execSqlSync("DELETE FROM pay_refund WHERE order_no = $1", orderNo);
    client->execSqlSync("DELETE FROM pay_payment WHERE payment_no = $1",
                        paymentNo);
    client->execSqlSync("DELETE FROM pay_order WHERE order_no = $1", orderNo);
}

DROGON_TEST(PayPlugin_Refund_NoWechatClient_ConsistentWriteback)
{
    Json::Value root;
    CHECK(loadConfig(root));
    CHECK(root.isMember("db_clients"));
    CHECK(root["db_clients"].isArray());
    CHECK(!root["db_clients"].empty());

    const auto &db = root["db_clients"][0];
    const std::string connInfo = buildPgConnInfo(db);
    CHECK(!connInfo.empty());

    auto client = drogon::orm::DbClient::newPgClient(connInfo, 1);
    CHECK(client != nullptr);

    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_order ("
        "id BIGSERIAL PRIMARY KEY,"
        "order_no VARCHAR(64) NOT NULL UNIQUE,"
        "user_id BIGINT NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "currency VARCHAR(16) NOT NULL,"
        "status VARCHAR(24) NOT NULL,"
        "channel VARCHAR(16) NOT NULL,"
        "title VARCHAR(128) NOT NULL,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");
    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_payment ("
        "id BIGSERIAL PRIMARY KEY,"
        "order_no VARCHAR(64) NOT NULL,"
        "payment_no VARCHAR(64) NOT NULL UNIQUE,"
        "channel_trade_no VARCHAR(64),"
        "status VARCHAR(24) NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "request_payload TEXT,"
        "response_payload TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");
    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_refund ("
        "id BIGSERIAL PRIMARY KEY,"
        "refund_no VARCHAR(64) NOT NULL UNIQUE,"
        "order_no VARCHAR(64) NOT NULL,"
        "payment_no VARCHAR(64) NOT NULL,"
        "channel_refund_no VARCHAR(64),"
        "status VARCHAR(24) NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "response_payload TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");

    const std::string orderNo = "ord_" + drogon::utils::getUuid();
    const std::string paymentNo = "pay_" + drogon::utils::getUuid();
    const std::string amount = "5.67";

    using PayOrder = drogon_model::pay_test::PayOrder;
    drogon::orm::Mapper<PayOrder> orderMapper(client);
    PayOrder order;
    order.setOrderNo(orderNo);
    order.setUserId(30011);
    order.setAmount(amount);
    order.setCurrency("CNY");
    order.setStatus("PAID");
    order.setChannel("wechat");
    order.setTitle("Refund No Wechat Client");
    order.setCreatedAt(trantor::Date::now());
    order.setUpdatedAt(trantor::Date::now());
    orderMapper.insert(order);

    using PayPayment = drogon_model::pay_test::PayPayment;
    drogon::orm::Mapper<PayPayment> paymentMapper(client);
    PayPayment payment;
    payment.setOrderNo(orderNo);
    payment.setPaymentNo(paymentNo);
    payment.setStatus("SUCCESS");
    payment.setAmount(amount);
    payment.setCreatedAt(trantor::Date::now());
    payment.setUpdatedAt(trantor::Date::now());
    paymentMapper.insert(payment);

    PayPlugin plugin;
    plugin.setTestClients(nullptr, client);

    Json::Value payload;
    payload["order_no"] = orderNo;
    payload["payment_no"] = paymentNo;
    payload["amount"] = amount;
    const std::string body = pay::utils::toJsonString(payload);

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    req->setBody(body);

    std::promise<drogon::HttpResponsePtr> promise;
    plugin.refund(
        req,
        [&promise](const drogon::HttpResponsePtr &resp) {
            promise.set_value(resp);
        });

    auto future = promise.get_future();
    CHECK(future.wait_for(std::chrono::seconds(5)) ==
          std::future_status::ready);
    const auto resp = future.get();
    CHECK(resp != nullptr);
    CHECK(resp->statusCode() == drogon::k501NotImplemented);
    const auto respJson = resp->getJsonObject();
    CHECK(respJson != nullptr);
    CHECK((*respJson)["order_no"].asString() == orderNo);
    CHECK((*respJson)["payment_no"].asString() == paymentNo);
    CHECK((*respJson)["amount"].asString() == amount);
    CHECK((*respJson)["status"].asString() == "REFUND_FAIL");
    CHECK((*respJson)["error"].asString() == "wechat client not ready");

    std::string refundStatus;
    std::string responsePayload;
    for (int i = 0; i < 20; ++i)
    {
        const auto rows = client->execSqlSync(
            "SELECT status, response_payload FROM pay_refund "
            "WHERE order_no = $1 AND payment_no = $2 "
            "ORDER BY created_at DESC LIMIT 1",
            orderNo,
            paymentNo);
        if (!rows.empty())
        {
            refundStatus = rows.front()["status"].as<std::string>();
            if (!rows.front()["response_payload"].isNull())
            {
                responsePayload =
                    rows.front()["response_payload"].as<std::string>();
            }
            if (refundStatus == "REFUND_FAIL" && !responsePayload.empty())
            {
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    CHECK(refundStatus == "REFUND_FAIL");
    CHECK(responsePayload.find("wechat client not ready") !=
          std::string::npos);

    client->execSqlSync("DELETE FROM pay_refund WHERE order_no = $1", orderNo);
    client->execSqlSync("DELETE FROM pay_payment WHERE payment_no = $1",
                        paymentNo);
    client->execSqlSync("DELETE FROM pay_order WHERE order_no = $1", orderNo);
}

DROGON_TEST(PayPlugin_Refund_IdempotencySnapshot_OnNoWechatClientError)
{
    Json::Value root;
    CHECK(loadConfig(root));
    CHECK(root.isMember("db_clients"));
    CHECK(root["db_clients"].isArray());
    CHECK(!root["db_clients"].empty());

    const auto &db = root["db_clients"][0];
    const std::string connInfo = buildPgConnInfo(db);
    CHECK(!connInfo.empty());

    auto client = drogon::orm::DbClient::newPgClient(connInfo, 1);
    CHECK(client != nullptr);

    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_order ("
        "id BIGSERIAL PRIMARY KEY,"
        "order_no VARCHAR(64) NOT NULL UNIQUE,"
        "user_id BIGINT NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "currency VARCHAR(16) NOT NULL,"
        "status VARCHAR(24) NOT NULL,"
        "channel VARCHAR(16) NOT NULL,"
        "title VARCHAR(128) NOT NULL,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");
    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_payment ("
        "id BIGSERIAL PRIMARY KEY,"
        "order_no VARCHAR(64) NOT NULL,"
        "payment_no VARCHAR(64) NOT NULL UNIQUE,"
        "channel_trade_no VARCHAR(64),"
        "status VARCHAR(24) NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "request_payload TEXT,"
        "response_payload TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");
    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_refund ("
        "id BIGSERIAL PRIMARY KEY,"
        "refund_no VARCHAR(64) NOT NULL UNIQUE,"
        "order_no VARCHAR(64) NOT NULL,"
        "payment_no VARCHAR(64) NOT NULL,"
        "channel_refund_no VARCHAR(64),"
        "status VARCHAR(24) NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "response_payload TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");
    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_idempotency ("
        "idempotency_key VARCHAR(64) PRIMARY KEY,"
        "request_hash TEXT NOT NULL,"
        "response_snapshot TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "expires_at TIMESTAMPTZ NOT NULL)");

    const std::string orderNo = "ord_" + drogon::utils::getUuid();
    const std::string paymentNo = "pay_" + drogon::utils::getUuid();
    const std::string amount = "5.68";
    const std::string idempotencyKey = "idem_" + drogon::utils::getUuid();

    using PayOrder = drogon_model::pay_test::PayOrder;
    drogon::orm::Mapper<PayOrder> orderMapper(client);
    PayOrder order;
    order.setOrderNo(orderNo);
    order.setUserId(30012);
    order.setAmount(amount);
    order.setCurrency("CNY");
    order.setStatus("PAID");
    order.setChannel("wechat");
    order.setTitle("Refund Idempotency Error Snapshot");
    order.setCreatedAt(trantor::Date::now());
    order.setUpdatedAt(trantor::Date::now());
    orderMapper.insert(order);

    using PayPayment = drogon_model::pay_test::PayPayment;
    drogon::orm::Mapper<PayPayment> paymentMapper(client);
    PayPayment payment;
    payment.setOrderNo(orderNo);
    payment.setPaymentNo(paymentNo);
    payment.setStatus("SUCCESS");
    payment.setAmount(amount);
    payment.setCreatedAt(trantor::Date::now());
    payment.setUpdatedAt(trantor::Date::now());
    paymentMapper.insert(payment);

    PayPlugin plugin;
    plugin.setTestClients(nullptr, client);

    Json::Value payload;
    payload["order_no"] = orderNo;
    payload["payment_no"] = paymentNo;
    payload["amount"] = amount;
    const std::string body = pay::utils::toJsonString(payload);

    auto makeReq = [&]() {
        auto req = drogon::HttpRequest::newHttpRequest();
        req->setMethod(drogon::Post);
        req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        req->setBody(body);
        req->addHeader("Idempotency-Key", idempotencyKey);
        return req;
    };

    std::promise<drogon::HttpResponsePtr> promise1;
    plugin.refund(
        makeReq(),
        [&promise1](const drogon::HttpResponsePtr &resp) {
            promise1.set_value(resp);
        });
    auto future1 = promise1.get_future();
    CHECK(future1.wait_for(std::chrono::seconds(5)) ==
          std::future_status::ready);
    const auto resp1 = future1.get();
    CHECK(resp1 != nullptr);
    CHECK(resp1->statusCode() == drogon::k501NotImplemented);
    const auto json1 = resp1->getJsonObject();
    CHECK(json1 != nullptr);
    CHECK((*json1)["status"].asString() == "REFUND_FAIL");
    CHECK((*json1)["error"].asString() == "wechat client not ready");

    std::promise<drogon::HttpResponsePtr> promise2;
    plugin.refund(
        makeReq(),
        [&promise2](const drogon::HttpResponsePtr &resp) {
            promise2.set_value(resp);
        });
    auto future2 = promise2.get_future();
    CHECK(future2.wait_for(std::chrono::seconds(5)) ==
          std::future_status::ready);
    const auto resp2 = future2.get();
    CHECK(resp2 != nullptr);
    CHECK(resp2->statusCode() == drogon::k200OK);
    const auto json2 = resp2->getJsonObject();
    CHECK(json2 != nullptr);
    CHECK((*json2)["status"].asString() == "REFUND_FAIL");
    CHECK((*json2)["error"].asString() == "wechat client not ready");
    CHECK((*json2)["order_no"].asString() == orderNo);
    CHECK((*json2)["payment_no"].asString() == paymentNo);

    const auto refundCountRows = client->execSqlSync(
        "SELECT COUNT(*) AS cnt FROM pay_refund WHERE order_no = $1 AND payment_no = $2",
        orderNo,
        paymentNo);
    CHECK(!refundCountRows.empty());
    CHECK(refundCountRows.front()["cnt"].as<int64_t>() == 1);

    const auto idempRows = client->execSqlSync(
        "SELECT response_snapshot FROM pay_idempotency WHERE idempotency_key = $1",
        "refund:" + idempotencyKey);
    CHECK(!idempRows.empty());
    CHECK(!idempRows.front()["response_snapshot"].isNull());
    const auto snapshotText =
        idempRows.front()["response_snapshot"].as<std::string>();
    CHECK(snapshotText.find("wechat client not ready") != std::string::npos);

    client->execSqlSync("DELETE FROM pay_idempotency WHERE idempotency_key = $1",
                        "refund:" + idempotencyKey);
    client->execSqlSync("DELETE FROM pay_refund WHERE order_no = $1", orderNo);
    client->execSqlSync("DELETE FROM pay_payment WHERE payment_no = $1",
                        paymentNo);
    client->execSqlSync("DELETE FROM pay_order WHERE order_no = $1", orderNo);
}

DROGON_TEST(PayPlugin_Refund_IdempotencySnapshot_OnWechatError)
{
    Json::Value root;
    CHECK(loadConfig(root));
    CHECK(root.isMember("db_clients"));
    CHECK(root["db_clients"].isArray());
    CHECK(!root["db_clients"].empty());

    const auto &db = root["db_clients"][0];
    const std::string connInfo = buildPgConnInfo(db);
    CHECK(!connInfo.empty());

    auto client = drogon::orm::DbClient::newPgClient(connInfo, 1);
    CHECK(client != nullptr);

    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_order ("
        "id BIGSERIAL PRIMARY KEY,"
        "order_no VARCHAR(64) NOT NULL UNIQUE,"
        "user_id BIGINT NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "currency VARCHAR(16) NOT NULL,"
        "status VARCHAR(24) NOT NULL,"
        "channel VARCHAR(16) NOT NULL,"
        "title VARCHAR(128) NOT NULL,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");
    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_payment ("
        "id BIGSERIAL PRIMARY KEY,"
        "order_no VARCHAR(64) NOT NULL,"
        "payment_no VARCHAR(64) NOT NULL UNIQUE,"
        "channel_trade_no VARCHAR(64),"
        "status VARCHAR(24) NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "request_payload TEXT,"
        "response_payload TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");
    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_refund ("
        "id BIGSERIAL PRIMARY KEY,"
        "refund_no VARCHAR(64) NOT NULL UNIQUE,"
        "order_no VARCHAR(64) NOT NULL,"
        "payment_no VARCHAR(64) NOT NULL,"
        "channel_refund_no VARCHAR(64),"
        "status VARCHAR(24) NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "response_payload TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");
    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_idempotency ("
        "idempotency_key VARCHAR(64) PRIMARY KEY,"
        "request_hash TEXT NOT NULL,"
        "response_snapshot TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "expires_at TIMESTAMPTZ NOT NULL)");

    const std::string orderNo = "ord_" + drogon::utils::getUuid();
    const std::string paymentNo = "pay_" + drogon::utils::getUuid();
    const std::string amount = "7.65";
    const std::string idempotencyKey = "idem_" + drogon::utils::getUuid();

    using PayOrder = drogon_model::pay_test::PayOrder;
    drogon::orm::Mapper<PayOrder> orderMapper(client);
    PayOrder order;
    order.setOrderNo(orderNo);
    order.setUserId(30013);
    order.setAmount(amount);
    order.setCurrency("CNY");
    order.setStatus("PAID");
    order.setChannel("wechat");
    order.setTitle("Refund Idempotency Wechat Error");
    order.setCreatedAt(trantor::Date::now());
    order.setUpdatedAt(trantor::Date::now());
    orderMapper.insert(order);

    using PayPayment = drogon_model::pay_test::PayPayment;
    drogon::orm::Mapper<PayPayment> paymentMapper(client);
    PayPayment payment;
    payment.setOrderNo(orderNo);
    payment.setPaymentNo(paymentNo);
    payment.setStatus("SUCCESS");
    payment.setAmount(amount);
    payment.setCreatedAt(trantor::Date::now());
    payment.setUpdatedAt(trantor::Date::now());
    paymentMapper.insert(payment);

    Json::Value wechatConfig;
    wechatConfig["api_base"] = "https://api.mch.weixin.qq.com";
    wechatConfig["mch_id"] = "";
    wechatConfig["serial_no"] = "";
    wechatConfig["private_key_path"] = "";
    wechatConfig["api_v3_key"] = "0123456789abcdef0123456789abcdef";
    auto wechatClient = std::make_shared<WechatPayClient>(wechatConfig);

    PayPlugin plugin;
    plugin.setTestClients(wechatClient, client);

    Json::Value payload;
    payload["order_no"] = orderNo;
    payload["payment_no"] = paymentNo;
    payload["amount"] = amount;
    const std::string body = pay::utils::toJsonString(payload);

    auto makeReq = [&]() {
        auto req = drogon::HttpRequest::newHttpRequest();
        req->setMethod(drogon::Post);
        req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        req->setBody(body);
        req->addHeader("Idempotency-Key", idempotencyKey);
        return req;
    };

    std::promise<drogon::HttpResponsePtr> promise1;
    plugin.refund(
        makeReq(),
        [&promise1](const drogon::HttpResponsePtr &resp) {
            promise1.set_value(resp);
        });
    auto future1 = promise1.get_future();
    CHECK(future1.wait_for(std::chrono::seconds(5)) ==
          std::future_status::ready);
    const auto resp1 = future1.get();
    CHECK(resp1 != nullptr);
    CHECK(resp1->statusCode() == drogon::k502BadGateway);
    const auto json1 = resp1->getJsonObject();
    CHECK(json1 != nullptr);
    CHECK((*json1)["status"].asString() == "REFUND_FAIL");
    CHECK((*json1)["error"].asString().find("wechat pay config missing") !=
          std::string::npos);

    std::promise<drogon::HttpResponsePtr> promise2;
    plugin.refund(
        makeReq(),
        [&promise2](const drogon::HttpResponsePtr &resp) {
            promise2.set_value(resp);
        });
    auto future2 = promise2.get_future();
    CHECK(future2.wait_for(std::chrono::seconds(5)) ==
          std::future_status::ready);
    const auto resp2 = future2.get();
    CHECK(resp2 != nullptr);
    CHECK(resp2->statusCode() == drogon::k200OK);
    const auto json2 = resp2->getJsonObject();
    CHECK(json2 != nullptr);
    CHECK((*json2)["status"].asString() == "REFUND_FAIL");
    CHECK((*json2)["error"].asString().find("wechat pay config missing") !=
          std::string::npos);
    CHECK((*json2)["order_no"].asString() == orderNo);
    CHECK((*json2)["payment_no"].asString() == paymentNo);

    const auto refundCountRows = client->execSqlSync(
        "SELECT COUNT(*) AS cnt FROM pay_refund WHERE order_no = $1 AND payment_no = $2",
        orderNo,
        paymentNo);
    CHECK(!refundCountRows.empty());
    CHECK(refundCountRows.front()["cnt"].as<int64_t>() == 1);

    const auto idempRows = client->execSqlSync(
        "SELECT response_snapshot FROM pay_idempotency WHERE idempotency_key = $1",
        "refund:" + idempotencyKey);
    CHECK(!idempRows.empty());
    CHECK(!idempRows.front()["response_snapshot"].isNull());
    const auto snapshotText =
        idempRows.front()["response_snapshot"].as<std::string>();
    CHECK(snapshotText.find("wechat pay config missing") != std::string::npos);

    client->execSqlSync("DELETE FROM pay_idempotency WHERE idempotency_key = $1",
                        "refund:" + idempotencyKey);
    client->execSqlSync("DELETE FROM pay_refund WHERE order_no = $1", orderNo);
    client->execSqlSync("DELETE FROM pay_payment WHERE payment_no = $1",
                        paymentNo);
    client->execSqlSync("DELETE FROM pay_order WHERE order_no = $1", orderNo);
}

DROGON_TEST(PayPlugin_Refund_DefaultPaymentNo)
{
    Json::Value root;
    CHECK(loadConfig(root));
    CHECK(root.isMember("db_clients"));
    CHECK(root["db_clients"].isArray());
    CHECK(!root["db_clients"].empty());

    const auto &db = root["db_clients"][0];
    const std::string connInfo = buildPgConnInfo(db);
    CHECK(!connInfo.empty());

    auto client = drogon::orm::DbClient::newPgClient(connInfo, 1);
    CHECK(client != nullptr);

    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_order ("
        "id BIGSERIAL PRIMARY KEY,"
        "order_no VARCHAR(64) NOT NULL UNIQUE,"
        "user_id BIGINT NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "currency VARCHAR(16) NOT NULL,"
        "status VARCHAR(24) NOT NULL,"
        "channel VARCHAR(16) NOT NULL,"
        "title VARCHAR(128) NOT NULL,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");
    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_payment ("
        "id BIGSERIAL PRIMARY KEY,"
        "order_no VARCHAR(64) NOT NULL,"
        "payment_no VARCHAR(64) NOT NULL UNIQUE,"
        "channel_trade_no VARCHAR(64),"
        "status VARCHAR(24) NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "request_payload TEXT,"
        "response_payload TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");
    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_refund ("
        "id BIGSERIAL PRIMARY KEY,"
        "refund_no VARCHAR(64) NOT NULL UNIQUE,"
        "order_no VARCHAR(64) NOT NULL,"
        "payment_no VARCHAR(64) NOT NULL,"
        "channel_refund_no VARCHAR(64),"
        "status VARCHAR(24) NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "response_payload TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");

    const std::string orderNo = "ord_" + drogon::utils::getUuid();
    const std::string paymentNo = "pay_" + drogon::utils::getUuid();
    const std::string amount = "7.77";

    using PayOrder = drogon_model::pay_test::PayOrder;
    drogon::orm::Mapper<PayOrder> orderMapper(client);
    PayOrder order;
    order.setOrderNo(orderNo);
    order.setUserId(30003);
    order.setAmount(amount);
    order.setCurrency("CNY");
    order.setStatus("PAID");
    order.setChannel("wechat");
    order.setTitle("Refund Default Payment");
    order.setCreatedAt(trantor::Date::now());
    order.setUpdatedAt(trantor::Date::now());
    orderMapper.insert(order);

    using PayPayment = drogon_model::pay_test::PayPayment;
    drogon::orm::Mapper<PayPayment> paymentMapper(client);
    PayPayment payment;
    payment.setOrderNo(orderNo);
    payment.setPaymentNo(paymentNo);
    payment.setStatus("SUCCESS");
    payment.setAmount(amount);
    payment.setCreatedAt(trantor::Date::now());
    payment.setUpdatedAt(trantor::Date::now());
    paymentMapper.insert(payment);

    const uint16_t port = 24090;
    drogon::app().addListener("127.0.0.1", port);
    drogon::app().registerHandler(
        "/v3/refund/domestic/refunds",
        [](const drogon::HttpRequestPtr &req,
           std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
            Json::Value respBody;
            respBody["status"] = "SUCCESS";
            respBody["refund_id"] = "wx_refund_default";
            auto resp = drogon::HttpResponse::newHttpJsonResponse(respBody);
            cb(resp);
        },
        {drogon::Post});

    std::thread serverThread([]() { drogon::app().run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    const auto tempDir = std::filesystem::temp_directory_path();
    const auto keyPath =
        tempDir / ("wechatpay_key_" + drogon::utils::getUuid() + ".pem");
    CHECK(writeTempPrivateKey(keyPath));

    Json::Value wechatConfig;
    wechatConfig["api_base"] =
        "http://127.0.0.1:" + std::to_string(port);
    wechatConfig["mch_id"] = "mch_123";
    wechatConfig["serial_no"] = "SERIAL_REFUND_DEF";
    wechatConfig["private_key_path"] = keyPath.string();
    wechatConfig["api_v3_key"] = "0123456789abcdef0123456789abcdef";
    wechatConfig["notify_url"] = "https://notify.invalid";
    auto wechatClient = std::make_shared<WechatPayClient>(wechatConfig);

    PayPlugin plugin;
    plugin.setTestClients(wechatClient, client);

    Json::Value payload;
    payload["order_no"] = orderNo;
    payload["amount"] = amount;
    const std::string body = pay::utils::toJsonString(payload);

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    req->setBody(body);

    std::promise<drogon::HttpResponsePtr> promise;
    plugin.refund(
        req,
        [&promise](const drogon::HttpResponsePtr &resp) {
            promise.set_value(resp);
        });

    auto future = promise.get_future();
    CHECK(future.wait_for(std::chrono::seconds(5)) ==
          std::future_status::ready);
    const auto resp = future.get();
    CHECK(resp != nullptr);
    CHECK(resp->statusCode() == drogon::k200OK);
    const auto respJson = resp->getJsonObject();
    CHECK(respJson != nullptr);
    CHECK((*respJson)["status"].asString() == "REFUND_SUCCESS");

    const auto refundRows = client->execSqlSync(
        "SELECT payment_no FROM pay_refund WHERE order_no = $1",
        orderNo);
    CHECK(refundRows.size() >= 1);
    CHECK(refundRows.front()["payment_no"].as<std::string>() == paymentNo);

    drogon::app().quit();
    if (serverThread.joinable())
    {
        serverThread.join();
    }

    std::error_code ec;
    std::filesystem::remove(keyPath, ec);
    client->execSqlSync("DELETE FROM pay_refund WHERE order_no = $1",
                        orderNo);
    client->execSqlSync("DELETE FROM pay_payment WHERE payment_no = $1",
                        paymentNo);
    client->execSqlSync("DELETE FROM pay_order WHERE order_no = $1", orderNo);
}

DROGON_TEST(PayPlugin_Refund_OrderNotPaid)
{
    Json::Value root;
    CHECK(loadConfig(root));
    CHECK(root.isMember("db_clients"));
    CHECK(root["db_clients"].isArray());
    CHECK(!root["db_clients"].empty());

    const auto &db = root["db_clients"][0];
    const std::string connInfo = buildPgConnInfo(db);
    CHECK(!connInfo.empty());

    auto client = drogon::orm::DbClient::newPgClient(connInfo, 1);
    CHECK(client != nullptr);

    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_order ("
        "id BIGSERIAL PRIMARY KEY,"
        "order_no VARCHAR(64) NOT NULL UNIQUE,"
        "user_id BIGINT NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "currency VARCHAR(16) NOT NULL,"
        "status VARCHAR(24) NOT NULL,"
        "channel VARCHAR(16) NOT NULL,"
        "title VARCHAR(128) NOT NULL,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");
    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_payment ("
        "id BIGSERIAL PRIMARY KEY,"
        "order_no VARCHAR(64) NOT NULL,"
        "payment_no VARCHAR(64) NOT NULL UNIQUE,"
        "channel_trade_no VARCHAR(64),"
        "status VARCHAR(24) NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "request_payload TEXT,"
        "response_payload TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");

    const std::string orderNo = "ord_" + drogon::utils::getUuid();
    const std::string paymentNo = "pay_" + drogon::utils::getUuid();
    const std::string amount = "6.66";

    using PayOrder = drogon_model::pay_test::PayOrder;
    drogon::orm::Mapper<PayOrder> orderMapper(client);
    PayOrder order;
    order.setOrderNo(orderNo);
    order.setUserId(30004);
    order.setAmount(amount);
    order.setCurrency("CNY");
    order.setStatus("PAYING");
    order.setChannel("wechat");
    order.setTitle("Refund Not Paid");
    order.setCreatedAt(trantor::Date::now());
    order.setUpdatedAt(trantor::Date::now());
    orderMapper.insert(order);

    using PayPayment = drogon_model::pay_test::PayPayment;
    drogon::orm::Mapper<PayPayment> paymentMapper(client);
    PayPayment payment;
    payment.setOrderNo(orderNo);
    payment.setPaymentNo(paymentNo);
    payment.setStatus("SUCCESS");
    payment.setAmount(amount);
    payment.setCreatedAt(trantor::Date::now());
    payment.setUpdatedAt(trantor::Date::now());
    paymentMapper.insert(payment);

    PayPlugin plugin;
    plugin.setTestClients(nullptr, client);

    Json::Value payload;
    payload["order_no"] = orderNo;
    payload["amount"] = amount;
    const std::string body = pay::utils::toJsonString(payload);

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    req->setBody(body);

    std::promise<drogon::HttpResponsePtr> promise;
    plugin.refund(
        req,
        [&promise](const drogon::HttpResponsePtr &resp) {
            promise.set_value(resp);
        });

    auto future = promise.get_future();
    CHECK(future.wait_for(std::chrono::seconds(5)) ==
          std::future_status::ready);
    const auto resp = future.get();
    CHECK(resp != nullptr);
    CHECK(resp->statusCode() == drogon::k409Conflict);
    CHECK(resp->body().find("order not paid") != std::string::npos);

    client->execSqlSync("DELETE FROM pay_payment WHERE payment_no = $1",
                        paymentNo);
    client->execSqlSync("DELETE FROM pay_order WHERE order_no = $1", orderNo);
}

DROGON_TEST(PayPlugin_Refund_PaymentNotSuccessful)
{
    Json::Value root;
    CHECK(loadConfig(root));
    CHECK(root.isMember("db_clients"));
    CHECK(root["db_clients"].isArray());
    CHECK(!root["db_clients"].empty());

    const auto &db = root["db_clients"][0];
    const std::string connInfo = buildPgConnInfo(db);
    CHECK(!connInfo.empty());

    auto client = drogon::orm::DbClient::newPgClient(connInfo, 1);
    CHECK(client != nullptr);

    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_order ("
        "id BIGSERIAL PRIMARY KEY,"
        "order_no VARCHAR(64) NOT NULL UNIQUE,"
        "user_id BIGINT NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "currency VARCHAR(16) NOT NULL,"
        "status VARCHAR(24) NOT NULL,"
        "channel VARCHAR(16) NOT NULL,"
        "title VARCHAR(128) NOT NULL,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");
    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_payment ("
        "id BIGSERIAL PRIMARY KEY,"
        "order_no VARCHAR(64) NOT NULL,"
        "payment_no VARCHAR(64) NOT NULL UNIQUE,"
        "channel_trade_no VARCHAR(64),"
        "status VARCHAR(24) NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "request_payload TEXT,"
        "response_payload TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");

    const std::string orderNo = "ord_" + drogon::utils::getUuid();
    const std::string paymentNo = "pay_" + drogon::utils::getUuid();
    const std::string amount = "6.66";

    using PayOrder = drogon_model::pay_test::PayOrder;
    drogon::orm::Mapper<PayOrder> orderMapper(client);
    PayOrder order;
    order.setOrderNo(orderNo);
    order.setUserId(30006);
    order.setAmount(amount);
    order.setCurrency("CNY");
    order.setStatus("PAID");
    order.setChannel("wechat");
    order.setTitle("Refund Payment Not Success");
    order.setCreatedAt(trantor::Date::now());
    order.setUpdatedAt(trantor::Date::now());
    orderMapper.insert(order);

    using PayPayment = drogon_model::pay_test::PayPayment;
    drogon::orm::Mapper<PayPayment> paymentMapper(client);
    PayPayment payment;
    payment.setOrderNo(orderNo);
    payment.setPaymentNo(paymentNo);
    payment.setStatus("PROCESSING");
    payment.setAmount(amount);
    payment.setCreatedAt(trantor::Date::now());
    payment.setUpdatedAt(trantor::Date::now());
    paymentMapper.insert(payment);

    PayPlugin plugin;
    plugin.setTestClients(nullptr, client);

    Json::Value payload;
    payload["order_no"] = orderNo;
    payload["payment_no"] = paymentNo;
    payload["amount"] = amount;
    const std::string body = pay::utils::toJsonString(payload);

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    req->setBody(body);

    std::promise<drogon::HttpResponsePtr> promise;
    plugin.refund(
        req,
        [&promise](const drogon::HttpResponsePtr &resp) {
            promise.set_value(resp);
        });

    auto future = promise.get_future();
    CHECK(future.wait_for(std::chrono::seconds(5)) ==
          std::future_status::ready);
    const auto resp = future.get();
    CHECK(resp != nullptr);
    CHECK(resp->statusCode() == drogon::k409Conflict);
    CHECK(resp->body().find("payment not successful") != std::string::npos);

    client->execSqlSync("DELETE FROM pay_payment WHERE payment_no = $1",
                        paymentNo);
    client->execSqlSync("DELETE FROM pay_order WHERE order_no = $1", orderNo);
}

DROGON_TEST(PayPlugin_Refund_DuplicateInProgress)
{
    Json::Value root;
    CHECK(loadConfig(root));
    CHECK(root.isMember("db_clients"));
    CHECK(root["db_clients"].isArray());
    CHECK(!root["db_clients"].empty());

    const auto &db = root["db_clients"][0];
    const std::string connInfo = buildPgConnInfo(db);
    CHECK(!connInfo.empty());

    auto client = drogon::orm::DbClient::newPgClient(connInfo, 1);
    CHECK(client != nullptr);

    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_order ("
        "id BIGSERIAL PRIMARY KEY,"
        "order_no VARCHAR(64) NOT NULL UNIQUE,"
        "user_id BIGINT NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "currency VARCHAR(16) NOT NULL,"
        "status VARCHAR(24) NOT NULL,"
        "channel VARCHAR(16) NOT NULL,"
        "title VARCHAR(128) NOT NULL,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");
    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_payment ("
        "id BIGSERIAL PRIMARY KEY,"
        "order_no VARCHAR(64) NOT NULL,"
        "payment_no VARCHAR(64) NOT NULL UNIQUE,"
        "channel_trade_no VARCHAR(64),"
        "status VARCHAR(24) NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "request_payload TEXT,"
        "response_payload TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");
    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_refund ("
        "id BIGSERIAL PRIMARY KEY,"
        "refund_no VARCHAR(64) NOT NULL UNIQUE,"
        "order_no VARCHAR(64) NOT NULL,"
        "payment_no VARCHAR(64) NOT NULL,"
        "channel_refund_no VARCHAR(64),"
        "status VARCHAR(24) NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "response_payload TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");

    const std::string orderNo = "ord_" + drogon::utils::getUuid();
    const std::string paymentNo = "pay_" + drogon::utils::getUuid();
    const std::string amount = "6.66";

    using PayOrder = drogon_model::pay_test::PayOrder;
    drogon::orm::Mapper<PayOrder> orderMapper(client);
    PayOrder order;
    order.setOrderNo(orderNo);
    order.setUserId(30007);
    order.setAmount(amount);
    order.setCurrency("CNY");
    order.setStatus("PAID");
    order.setChannel("wechat");
    order.setTitle("Refund Duplicate InProgress");
    order.setCreatedAt(trantor::Date::now());
    order.setUpdatedAt(trantor::Date::now());
    orderMapper.insert(order);

    using PayPayment = drogon_model::pay_test::PayPayment;
    drogon::orm::Mapper<PayPayment> paymentMapper(client);
    PayPayment payment;
    payment.setOrderNo(orderNo);
    payment.setPaymentNo(paymentNo);
    payment.setStatus("SUCCESS");
    payment.setAmount(amount);
    payment.setCreatedAt(trantor::Date::now());
    payment.setUpdatedAt(trantor::Date::now());
    paymentMapper.insert(payment);

    using PayRefund = drogon_model::pay_test::PayRefund;
    drogon::orm::Mapper<PayRefund> refundMapper(client);
    PayRefund refund;
    refund.setRefundNo("refund_" + drogon::utils::getUuid());
    refund.setOrderNo(orderNo);
    refund.setPaymentNo(paymentNo);
    refund.setStatus("REFUNDING");
    refund.setAmount(amount);
    refund.setCreatedAt(trantor::Date::now());
    refund.setUpdatedAt(trantor::Date::now());
    refundMapper.insert(refund);

    PayPlugin plugin;
    plugin.setTestClients(nullptr, client);

    Json::Value payload;
    payload["order_no"] = orderNo;
    payload["payment_no"] = paymentNo;
    payload["amount"] = amount;
    const std::string body = pay::utils::toJsonString(payload);

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    req->setBody(body);

    std::promise<drogon::HttpResponsePtr> promise;
    plugin.refund(
        req,
        [&promise](const drogon::HttpResponsePtr &resp) {
            promise.set_value(resp);
        });

    auto future = promise.get_future();
    CHECK(future.wait_for(std::chrono::seconds(5)) ==
          std::future_status::ready);
    const auto resp = future.get();
    CHECK(resp != nullptr);
    CHECK(resp->statusCode() == drogon::k409Conflict);
    CHECK(resp->body().find("refund already in progress") != std::string::npos);

    const auto countRows = client->execSqlSync(
        "SELECT COUNT(*) AS cnt FROM pay_refund WHERE order_no = $1",
        orderNo);
    CHECK(!countRows.empty());
    CHECK(countRows.front()["cnt"].as<int64_t>() == 1);

    client->execSqlSync("DELETE FROM pay_refund WHERE order_no = $1", orderNo);
    client->execSqlSync("DELETE FROM pay_payment WHERE payment_no = $1",
                        paymentNo);
    client->execSqlSync("DELETE FROM pay_order WHERE order_no = $1", orderNo);
}

DROGON_TEST(PayPlugin_Refund_IdempotentSuccessSnapshot)
{
    Json::Value root;
    CHECK(loadConfig(root));
    CHECK(root.isMember("db_clients"));
    CHECK(root["db_clients"].isArray());
    CHECK(!root["db_clients"].empty());

    const auto &db = root["db_clients"][0];
    const std::string connInfo = buildPgConnInfo(db);
    CHECK(!connInfo.empty());

    auto client = drogon::orm::DbClient::newPgClient(connInfo, 1);
    CHECK(client != nullptr);

    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_order ("
        "id BIGSERIAL PRIMARY KEY,"
        "order_no VARCHAR(64) NOT NULL UNIQUE,"
        "user_id BIGINT NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "currency VARCHAR(16) NOT NULL,"
        "status VARCHAR(24) NOT NULL,"
        "channel VARCHAR(16) NOT NULL,"
        "title VARCHAR(128) NOT NULL,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");
    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_payment ("
        "id BIGSERIAL PRIMARY KEY,"
        "order_no VARCHAR(64) NOT NULL,"
        "payment_no VARCHAR(64) NOT NULL UNIQUE,"
        "channel_trade_no VARCHAR(64),"
        "status VARCHAR(24) NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "request_payload TEXT,"
        "response_payload TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");
    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_refund ("
        "id BIGSERIAL PRIMARY KEY,"
        "refund_no VARCHAR(64) NOT NULL UNIQUE,"
        "order_no VARCHAR(64) NOT NULL,"
        "payment_no VARCHAR(64) NOT NULL,"
        "channel_refund_no VARCHAR(64),"
        "status VARCHAR(24) NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "response_payload TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");

    const std::string orderNo = "ord_" + drogon::utils::getUuid();
    const std::string paymentNo = "pay_" + drogon::utils::getUuid();
    const std::string amount = "6.66";
    const std::string historyRefundNo = "refund_" + drogon::utils::getUuid();
    const std::string channelRefundNo = "wx_refund_" + drogon::utils::getUuid();
    Json::Value historyPayloadJson;
    historyPayloadJson["status"] = "SUCCESS";
    historyPayloadJson["refund_id"] = channelRefundNo;
    historyPayloadJson["from"] = "snapshot";
    const std::string historyPayload = pay::utils::toJsonString(historyPayloadJson);

    using PayOrder = drogon_model::pay_test::PayOrder;
    drogon::orm::Mapper<PayOrder> orderMapper(client);
    PayOrder order;
    order.setOrderNo(orderNo);
    order.setUserId(30008);
    order.setAmount(amount);
    order.setCurrency("CNY");
    order.setStatus("PAID");
    order.setChannel("wechat");
    order.setTitle("Refund Idempotent Snapshot");
    order.setCreatedAt(trantor::Date::now());
    order.setUpdatedAt(trantor::Date::now());
    orderMapper.insert(order);

    using PayPayment = drogon_model::pay_test::PayPayment;
    drogon::orm::Mapper<PayPayment> paymentMapper(client);
    PayPayment payment;
    payment.setOrderNo(orderNo);
    payment.setPaymentNo(paymentNo);
    payment.setStatus("SUCCESS");
    payment.setAmount(amount);
    payment.setCreatedAt(trantor::Date::now());
    payment.setUpdatedAt(trantor::Date::now());
    paymentMapper.insert(payment);

    client->execSqlSync(
        "INSERT INTO pay_refund "
        "(refund_no, order_no, payment_no, channel_refund_no, status, amount, response_payload, created_at, updated_at) "
        "VALUES ($1, $2, $3, $4, $5, $6, $7, NOW(), NOW())",
        historyRefundNo,
        orderNo,
        paymentNo,
        channelRefundNo,
        "REFUND_SUCCESS",
        amount,
        historyPayload);

    PayPlugin plugin;
    plugin.setTestClients(nullptr, client);

    Json::Value payload;
    payload["order_no"] = orderNo;
    payload["payment_no"] = paymentNo;
    payload["amount"] = amount;
    const std::string body = pay::utils::toJsonString(payload);

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    req->setBody(body);

    std::promise<drogon::HttpResponsePtr> promise;
    plugin.refund(
        req,
        [&promise](const drogon::HttpResponsePtr &resp) {
            promise.set_value(resp);
        });

    auto future = promise.get_future();
    CHECK(future.wait_for(std::chrono::seconds(5)) ==
          std::future_status::ready);
    const auto resp = future.get();
    CHECK(resp != nullptr);
    CHECK(resp->statusCode() == drogon::k200OK);
    const auto respJson = resp->getJsonObject();
    CHECK(respJson != nullptr);
    CHECK((*respJson)["refund_no"].asString() == historyRefundNo);
    CHECK((*respJson)["order_no"].asString() == orderNo);
    CHECK((*respJson)["payment_no"].asString() == paymentNo);
    CHECK((*respJson)["amount"].asString() == amount);
    CHECK((*respJson)["status"].asString() == "REFUND_SUCCESS");
    CHECK((*respJson)["channel_refund_no"].asString() == channelRefundNo);
    CHECK((*respJson)["wechat_response"]["status"].asString() == "SUCCESS");
    CHECK((*respJson)["wechat_response"]["refund_id"].asString() ==
          channelRefundNo);

    const auto countRows = client->execSqlSync(
        "SELECT COUNT(*) AS cnt FROM pay_refund WHERE order_no = $1",
        orderNo);
    CHECK(!countRows.empty());
    CHECK(countRows.front()["cnt"].as<int64_t>() == 1);

    client->execSqlSync("DELETE FROM pay_refund WHERE order_no = $1", orderNo);
    client->execSqlSync("DELETE FROM pay_payment WHERE payment_no = $1",
                        paymentNo);
    client->execSqlSync("DELETE FROM pay_order WHERE order_no = $1", orderNo);
}

DROGON_TEST(PayPlugin_Refund_AmountExceedsPaid)
{
    Json::Value root;
    CHECK(loadConfig(root));
    CHECK(root.isMember("db_clients"));
    CHECK(root["db_clients"].isArray());
    CHECK(!root["db_clients"].empty());

    const auto &db = root["db_clients"][0];
    const std::string connInfo = buildPgConnInfo(db);
    CHECK(!connInfo.empty());

    auto client = drogon::orm::DbClient::newPgClient(connInfo, 1);
    CHECK(client != nullptr);

    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_order ("
        "id BIGSERIAL PRIMARY KEY,"
        "order_no VARCHAR(64) NOT NULL UNIQUE,"
        "user_id BIGINT NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "currency VARCHAR(16) NOT NULL,"
        "status VARCHAR(24) NOT NULL,"
        "channel VARCHAR(16) NOT NULL,"
        "title VARCHAR(128) NOT NULL,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");
    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_payment ("
        "id BIGSERIAL PRIMARY KEY,"
        "order_no VARCHAR(64) NOT NULL,"
        "payment_no VARCHAR(64) NOT NULL UNIQUE,"
        "channel_trade_no VARCHAR(64),"
        "status VARCHAR(24) NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "request_payload TEXT,"
        "response_payload TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");
    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_refund ("
        "id BIGSERIAL PRIMARY KEY,"
        "refund_no VARCHAR(64) NOT NULL UNIQUE,"
        "order_no VARCHAR(64) NOT NULL,"
        "payment_no VARCHAR(64) NOT NULL,"
        "channel_refund_no VARCHAR(64),"
        "status VARCHAR(24) NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "response_payload TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");

    const std::string orderNo = "ord_" + drogon::utils::getUuid();
    const std::string paymentNo = "pay_" + drogon::utils::getUuid();
    const std::string amount = "10.00";

    using PayOrder = drogon_model::pay_test::PayOrder;
    drogon::orm::Mapper<PayOrder> orderMapper(client);
    PayOrder order;
    order.setOrderNo(orderNo);
    order.setUserId(30005);
    order.setAmount(amount);
    order.setCurrency("CNY");
    order.setStatus("PAID");
    order.setChannel("wechat");
    order.setTitle("Refund Exceed");
    order.setCreatedAt(trantor::Date::now());
    order.setUpdatedAt(trantor::Date::now());
    orderMapper.insert(order);

    using PayPayment = drogon_model::pay_test::PayPayment;
    drogon::orm::Mapper<PayPayment> paymentMapper(client);
    PayPayment payment;
    payment.setOrderNo(orderNo);
    payment.setPaymentNo(paymentNo);
    payment.setStatus("SUCCESS");
    payment.setAmount(amount);
    payment.setCreatedAt(trantor::Date::now());
    payment.setUpdatedAt(trantor::Date::now());
    paymentMapper.insert(payment);

    using PayRefund = drogon_model::pay_test::PayRefund;
    drogon::orm::Mapper<PayRefund> refundMapper(client);
    PayRefund refund;
    refund.setRefundNo("refund_" + drogon::utils::getUuid());
    refund.setOrderNo(orderNo);
    refund.setPaymentNo(paymentNo);
    refund.setStatus("REFUND_SUCCESS");
    refund.setAmount("6.00");
    refund.setCreatedAt(trantor::Date::now());
    refund.setUpdatedAt(trantor::Date::now());
    refundMapper.insert(refund);

    PayPlugin plugin;
    plugin.setTestClients(nullptr, client);

    Json::Value payload;
    payload["order_no"] = orderNo;
    payload["payment_no"] = paymentNo;
    payload["amount"] = "5.00";
    const std::string body = pay::utils::toJsonString(payload);

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    req->setBody(body);

    std::promise<drogon::HttpResponsePtr> promise;
    plugin.refund(
        req,
        [&promise](const drogon::HttpResponsePtr &resp) {
            promise.set_value(resp);
        });

    auto future = promise.get_future();
    CHECK(future.wait_for(std::chrono::seconds(5)) ==
          std::future_status::ready);
    const auto resp = future.get();
    CHECK(resp != nullptr);
    CHECK(resp->statusCode() == drogon::k409Conflict);
    CHECK(resp->body().find("refund amount exceeds paid") != std::string::npos);

    const auto countRows = client->execSqlSync(
        "SELECT COUNT(*) AS cnt FROM pay_refund WHERE order_no = $1",
        orderNo);
    CHECK(!countRows.empty());
    CHECK(countRows.front()["cnt"].as<int64_t>() == 1);

    client->execSqlSync("DELETE FROM pay_refund WHERE order_no = $1", orderNo);
    client->execSqlSync("DELETE FROM pay_payment WHERE payment_no = $1",
                        paymentNo);
    client->execSqlSync("DELETE FROM pay_order WHERE order_no = $1", orderNo);
}

DROGON_TEST(PayPlugin_Refund_ReasonTooLong)
{
    Json::Value root;
    CHECK(loadConfig(root));
    CHECK(root.isMember("db_clients"));
    CHECK(root["db_clients"].isArray());
    CHECK(!root["db_clients"].empty());

    const auto &db = root["db_clients"][0];
    const std::string connInfo = buildPgConnInfo(db);
    CHECK(!connInfo.empty());

    auto client = drogon::orm::DbClient::newPgClient(connInfo, 1);
    CHECK(client != nullptr);

    PayPlugin plugin;
    plugin.setTestClients(nullptr, client);

    Json::Value payload;
    payload["order_no"] = "ord_" + drogon::utils::getUuid();
    payload["amount"] = "1.00";
    payload["reason"] = std::string(81, 'x');
    const std::string body = pay::utils::toJsonString(payload);

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    req->setBody(body);

    std::promise<drogon::HttpResponsePtr> promise;
    plugin.refund(
        req,
        [&promise](const drogon::HttpResponsePtr &resp) {
            promise.set_value(resp);
        });

    auto future = promise.get_future();
    if (future.wait_for(std::chrono::seconds(5)) !=
        std::future_status::ready)
    {
        return;
    }
    const auto resp = future.get();
    if (!resp)
    {
        FAIL("response is null");
        return;
    }
    if (resp->statusCode() != drogon::k400BadRequest)
    {
        FAIL("unexpected status: ", static_cast<int>(resp->statusCode()),
             " body: ", resp->body());
        return;
    }
    if (resp->body().find("reason too long") == std::string::npos)
    {
        FAIL("unexpected body: ", resp->body());
        return;
    }
}

DROGON_TEST(PayPlugin_Refund_InvalidFundsAccount)
{
    Json::Value root;
    CHECK(loadConfig(root));
    CHECK(root.isMember("db_clients"));
    CHECK(root["db_clients"].isArray());
    CHECK(!root["db_clients"].empty());

    const auto &db = root["db_clients"][0];
    const std::string connInfo = buildPgConnInfo(db);
    CHECK(!connInfo.empty());

    auto client = drogon::orm::DbClient::newPgClient(connInfo, 1);
    CHECK(client != nullptr);

    PayPlugin plugin;
    plugin.setTestClients(nullptr, client);

    Json::Value payload;
    payload["order_no"] = "ord_" + drogon::utils::getUuid();
    payload["amount"] = "1.00";
    payload["funds_account"] = "BAD_ACCOUNT";
    const std::string body = pay::utils::toJsonString(payload);

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    req->setBody(body);

    std::promise<drogon::HttpResponsePtr> promise;
    plugin.refund(
        req,
        [&promise](const drogon::HttpResponsePtr &resp) {
            promise.set_value(resp);
        });

    auto future = promise.get_future();
    CHECK(future.wait_for(std::chrono::seconds(5)) ==
          std::future_status::ready);
    const auto resp = future.get();
    CHECK(resp != nullptr);
    CHECK(resp->statusCode() == drogon::k400BadRequest);
    CHECK(resp->body().find("invalid funds_account") != std::string::npos);
}

DROGON_TEST(PayPlugin_Refund_InvalidNotifyUrl)
{
    Json::Value root;
    CHECK(loadConfig(root));
    CHECK(root.isMember("db_clients"));
    CHECK(root["db_clients"].isArray());
    CHECK(!root["db_clients"].empty());

    const auto &db = root["db_clients"][0];
    const std::string connInfo = buildPgConnInfo(db);
    CHECK(!connInfo.empty());

    auto client = drogon::orm::DbClient::newPgClient(connInfo, 1);
    CHECK(client != nullptr);

    PayPlugin plugin;
    plugin.setTestClients(nullptr, client);

    Json::Value payload;
    payload["order_no"] = "ord_" + drogon::utils::getUuid();
    payload["amount"] = "1.00";
    payload["notify_url"] = "ftp://invalid-url";
    const std::string body = pay::utils::toJsonString(payload);

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    req->setBody(body);

    std::promise<drogon::HttpResponsePtr> promise;
    plugin.refund(
        req,
        [&promise](const drogon::HttpResponsePtr &resp) {
            promise.set_value(resp);
        });

    auto future = promise.get_future();
    CHECK(future.wait_for(std::chrono::seconds(5)) ==
          std::future_status::ready);
    const auto resp = future.get();
    CHECK(resp != nullptr);
    CHECK(resp->statusCode() == drogon::k400BadRequest);
    CHECK(resp->body().find("invalid notify_url") != std::string::npos);
}

DROGON_TEST(PayPlugin_QueryRefund_WechatSuccess)
{
    Json::Value root;
    CHECK(loadConfig(root));
    CHECK(root.isMember("db_clients"));
    CHECK(root["db_clients"].isArray());
    CHECK(!root["db_clients"].empty());

    const auto &db = root["db_clients"][0];
    const std::string connInfo = buildPgConnInfo(db);
    CHECK(!connInfo.empty());

    auto client = drogon::orm::DbClient::newPgClient(connInfo, 1);
    CHECK(client != nullptr);

    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_refund ("
        "id BIGSERIAL PRIMARY KEY,"
        "refund_no VARCHAR(64) NOT NULL UNIQUE,"
        "order_no VARCHAR(64) NOT NULL,"
        "payment_no VARCHAR(64) NOT NULL,"
        "channel_refund_no VARCHAR(64),"
        "status VARCHAR(24) NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "response_payload TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");
    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_order ("
        "id BIGSERIAL PRIMARY KEY,"
        "order_no VARCHAR(64) NOT NULL UNIQUE,"
        "user_id BIGINT NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "currency VARCHAR(16) NOT NULL,"
        "status VARCHAR(24) NOT NULL,"
        "channel VARCHAR(16) NOT NULL,"
        "title VARCHAR(128) NOT NULL,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");
    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_ledger ("
        "id BIGSERIAL PRIMARY KEY,"
        "user_id BIGINT NOT NULL,"
        "order_no VARCHAR(64) NOT NULL,"
        "payment_no VARCHAR(64),"
        "entry_type VARCHAR(16) NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");

    const std::string refundNo = "refund_" + drogon::utils::getUuid();
    const std::string orderNo = "ord_" + drogon::utils::getUuid();
    const std::string paymentNo = "pay_" + drogon::utils::getUuid();
    const std::string amount = "8.88";

    using PayOrder = drogon_model::pay_test::PayOrder;
    drogon::orm::Mapper<PayOrder> orderMapper(client);
    PayOrder order;
    order.setOrderNo(orderNo);
    order.setUserId(30001);
    order.setAmount(amount);
    order.setCurrency("CNY");
    order.setStatus("PAYING");
    order.setChannel("wechat");
    order.setTitle("Refund Order");
    order.setCreatedAt(trantor::Date::now());
    order.setUpdatedAt(trantor::Date::now());
    orderMapper.insert(order);

    using PayRefund = drogon_model::pay_test::PayRefund;
    drogon::orm::Mapper<PayRefund> refundMapper(client);
    PayRefund refund;
    refund.setRefundNo(refundNo);
    refund.setOrderNo(orderNo);
    refund.setPaymentNo(paymentNo);
    refund.setStatus("REFUNDING");
    refund.setAmount(amount);
    refund.setCreatedAt(trantor::Date::now());
    refund.setUpdatedAt(trantor::Date::now());
    refundMapper.insert(refund);

    const uint16_t port = 24080;
    drogon::app().addListener("127.0.0.1", port);
    drogon::app().registerHandler(
        "/v3/refund/domestic/refunds/{1}",
        [refundNo](const drogon::HttpRequestPtr &req,
                   std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                   const std::string &param) {
            Json::Value body;
            body["status"] = "SUCCESS";
            body["refund_id"] = "wx_refund_1";
            body["out_refund_no"] = param;
            auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
            cb(resp);
        },
        {drogon::Get});

    std::thread serverThread([]() { drogon::app().run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    const auto tempDir = std::filesystem::temp_directory_path();
    const auto keyPath =
        tempDir / ("wechatpay_key_" + drogon::utils::getUuid() + ".pem");
    CHECK(writeTempPrivateKey(keyPath));

    Json::Value wechatConfig;
    wechatConfig["api_base"] =
        "http://127.0.0.1:" + std::to_string(port);
    wechatConfig["mch_id"] = "mch_123";
    wechatConfig["serial_no"] = "SERIAL_TEST";
    wechatConfig["private_key_path"] = keyPath.string();
    wechatConfig["api_v3_key"] = "0123456789abcdef0123456789abcdef";
    auto wechatClient = std::make_shared<WechatPayClient>(wechatConfig);

    PayPlugin plugin;
    plugin.setTestClients(wechatClient, client);

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setParameter("refund_no", refundNo);

    std::promise<drogon::HttpResponsePtr> promise;
    plugin.queryRefund(
        req,
        [&promise](const drogon::HttpResponsePtr &resp) {
            promise.set_value(resp);
        });

    auto future = promise.get_future();
    CHECK(future.wait_for(std::chrono::seconds(5)) ==
          std::future_status::ready);
    const auto resp = future.get();
    CHECK(resp != nullptr);
    CHECK(resp->statusCode() == drogon::k200OK);
    const auto respJson = resp->getJsonObject();
    CHECK(respJson != nullptr);
    CHECK((*respJson)["refund_no"].asString() == refundNo);
    CHECK((*respJson)["status"].asString() == "REFUND_SUCCESS");
    CHECK((*respJson)["channel_refund_no"].asString() == "wx_refund_1");
    CHECK((*respJson)["updated_at"].isString());
    CHECK((*respJson)["updated_at"].asString().find("T") != std::string::npos);
    CHECK((*respJson)["wechat_response"]["status"].asString() == "SUCCESS");

    const auto updated = refundMapper.findByPrimaryKey(
        refund.getValueOfId());
    CHECK(updated.getValueOfStatus() == "REFUND_SUCCESS");
    CHECK(updated.getValueOfChannelRefundNo() == "wx_refund_1");

    int64_t payloadReady = 0;
    for (int i = 0; i < 20; ++i)
    {
        const auto payloadRows = client->execSqlSync(
            "SELECT response_payload FROM pay_refund WHERE refund_no = $1",
            refundNo);
        if (!payloadRows.empty() && !payloadRows.front()["response_payload"].isNull())
        {
            const auto payload =
                payloadRows.front()["response_payload"].as<std::string>();
            if (payload.find("wx_refund_1") != std::string::npos)
            {
                payloadReady = 1;
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    CHECK(payloadReady == 1);

    int64_t ledgerCount = 0;
    for (int i = 0; i < 20; ++i)
    {
        const auto ledgerRows = client->execSqlSync(
            "SELECT COUNT(*) AS cnt FROM pay_ledger WHERE order_no = $1",
            orderNo);
        CHECK(!ledgerRows.empty());
        ledgerCount = ledgerRows.front()["cnt"].as<int64_t>();
        if (ledgerCount == 1)
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    CHECK(ledgerCount == 1);

    drogon::app().quit();
    if (serverThread.joinable())
    {
        serverThread.join();
    }

    std::error_code ec;
    std::filesystem::remove(keyPath, ec);
    client->execSqlSync("DELETE FROM pay_ledger WHERE order_no = $1", orderNo);
    client->execSqlSync("DELETE FROM pay_refund WHERE refund_no = $1",
                        refundNo);
    client->execSqlSync("DELETE FROM pay_order WHERE order_no = $1", orderNo);
}

DROGON_TEST(PayPlugin_QueryRefund_WechatProcessing)
{
    Json::Value root;
    CHECK(loadConfig(root));
    CHECK(root.isMember("db_clients"));
    CHECK(root["db_clients"].isArray());
    CHECK(!root["db_clients"].empty());

    const auto &db = root["db_clients"][0];
    const std::string connInfo = buildPgConnInfo(db);
    CHECK(!connInfo.empty());

    auto client = drogon::orm::DbClient::newPgClient(connInfo, 1);
    CHECK(client != nullptr);

    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_refund ("
        "id BIGSERIAL PRIMARY KEY,"
        "refund_no VARCHAR(64) NOT NULL UNIQUE,"
        "order_no VARCHAR(64) NOT NULL,"
        "payment_no VARCHAR(64) NOT NULL,"
        "channel_refund_no VARCHAR(64),"
        "status VARCHAR(24) NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "response_payload TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");
    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_ledger ("
        "id BIGSERIAL PRIMARY KEY,"
        "user_id BIGINT NOT NULL,"
        "order_no VARCHAR(64) NOT NULL,"
        "payment_no VARCHAR(64),"
        "entry_type VARCHAR(16) NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");

    const std::string refundNo = "refund_" + drogon::utils::getUuid();
    const std::string orderNo = "ord_" + drogon::utils::getUuid();
    const std::string paymentNo = "pay_" + drogon::utils::getUuid();
    const std::string amount = "6.66";

    using PayRefund = drogon_model::pay_test::PayRefund;
    drogon::orm::Mapper<PayRefund> refundMapper(client);
    PayRefund refund;
    refund.setRefundNo(refundNo);
    refund.setOrderNo(orderNo);
    refund.setPaymentNo(paymentNo);
    refund.setStatus("REFUNDING");
    refund.setAmount(amount);
    refund.setCreatedAt(trantor::Date::now());
    refund.setUpdatedAt(trantor::Date::now());
    refundMapper.insert(refund);

    const uint16_t port = 24085;
    drogon::app().addListener("127.0.0.1", port);
    drogon::app().registerHandler(
        "/v3/refund/domestic/refunds/{1}",
        [](const drogon::HttpRequestPtr &req,
           std::function<void(const drogon::HttpResponsePtr &)> &&cb,
           const std::string &param) {
            Json::Value body;
            body["status"] = "PROCESSING";
            body["refund_id"] = "wx_refund_processing";
            body["out_refund_no"] = param;
            auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
            cb(resp);
        },
        {drogon::Get});

    std::thread serverThread([]() { drogon::app().run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    const auto tempDir = std::filesystem::temp_directory_path();
    const auto keyPath =
        tempDir / ("wechatpay_key_" + drogon::utils::getUuid() + ".pem");
    CHECK(writeTempPrivateKey(keyPath));

    Json::Value wechatConfig;
    wechatConfig["api_base"] =
        "http://127.0.0.1:" + std::to_string(port);
    wechatConfig["mch_id"] = "mch_333";
    wechatConfig["serial_no"] = "SERIAL_REFUND_PROCESSING";
    wechatConfig["private_key_path"] = keyPath.string();
    wechatConfig["api_v3_key"] = "0123456789abcdef0123456789abcdef";
    auto wechatClient = std::make_shared<WechatPayClient>(wechatConfig);

    PayPlugin plugin;
    plugin.setTestClients(wechatClient, client);

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setParameter("refund_no", refundNo);

    std::promise<drogon::HttpResponsePtr> promise;
    plugin.queryRefund(
        req,
        [&promise](const drogon::HttpResponsePtr &resp) {
            promise.set_value(resp);
        });

    auto future = promise.get_future();
    CHECK(future.wait_for(std::chrono::seconds(5)) ==
          std::future_status::ready);
    const auto resp = future.get();
    CHECK(resp != nullptr);
    CHECK(resp->statusCode() == drogon::k200OK);
    const auto respJson = resp->getJsonObject();
    CHECK(respJson != nullptr);
    CHECK((*respJson)["refund_no"].asString() == refundNo);
    CHECK((*respJson)["status"].asString() == "REFUNDING");
    CHECK((*respJson)["wechat_response"]["status"].asString() ==
          "PROCESSING");

    const auto updated =
        refundMapper.findByPrimaryKey(refund.getValueOfId());
    CHECK(updated.getValueOfStatus() == "REFUNDING");
    CHECK(updated.getValueOfChannelRefundNo() == "wx_refund_processing");

    const auto ledgerRows = client->execSqlSync(
        "SELECT COUNT(*) AS cnt FROM pay_ledger WHERE order_no = $1",
        orderNo);
    CHECK(!ledgerRows.empty());
    CHECK(ledgerRows.front()["cnt"].as<int64_t>() == 0);

    drogon::app().quit();
    if (serverThread.joinable())
    {
        serverThread.join();
    }

    std::error_code ec;
    std::filesystem::remove(keyPath, ec);
    client->execSqlSync("DELETE FROM pay_refund WHERE refund_no = $1",
                        refundNo);
    client->execSqlSync("DELETE FROM pay_ledger WHERE order_no = $1", orderNo);
}

DROGON_TEST(PayPlugin_QueryRefund_WechatClosed)
{
    Json::Value root;
    CHECK(loadConfig(root));
    CHECK(root.isMember("db_clients"));
    CHECK(root["db_clients"].isArray());
    CHECK(!root["db_clients"].empty());

    const auto &db = root["db_clients"][0];
    const std::string connInfo = buildPgConnInfo(db);
    CHECK(!connInfo.empty());

    auto client = drogon::orm::DbClient::newPgClient(connInfo, 1);
    CHECK(client != nullptr);

    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_refund ("
        "id BIGSERIAL PRIMARY KEY,"
        "refund_no VARCHAR(64) NOT NULL UNIQUE,"
        "order_no VARCHAR(64) NOT NULL,"
        "payment_no VARCHAR(64) NOT NULL,"
        "channel_refund_no VARCHAR(64),"
        "status VARCHAR(24) NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "response_payload TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");
    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_ledger ("
        "id BIGSERIAL PRIMARY KEY,"
        "user_id BIGINT NOT NULL,"
        "order_no VARCHAR(64) NOT NULL,"
        "payment_no VARCHAR(64),"
        "entry_type VARCHAR(16) NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");

    const std::string refundNo = "refund_" + drogon::utils::getUuid();
    const std::string orderNo = "ord_" + drogon::utils::getUuid();
    const std::string paymentNo = "pay_" + drogon::utils::getUuid();
    const std::string amount = "3.21";

    using PayRefund = drogon_model::pay_test::PayRefund;
    drogon::orm::Mapper<PayRefund> refundMapper(client);
    PayRefund refund;
    refund.setRefundNo(refundNo);
    refund.setOrderNo(orderNo);
    refund.setPaymentNo(paymentNo);
    refund.setStatus("REFUNDING");
    refund.setAmount(amount);
    refund.setCreatedAt(trantor::Date::now());
    refund.setUpdatedAt(trantor::Date::now());
    refundMapper.insert(refund);

    const uint16_t port = 24086;
    drogon::app().addListener("127.0.0.1", port);
    drogon::app().registerHandler(
        "/v3/refund/domestic/refunds/{1}",
        [](const drogon::HttpRequestPtr &req,
           std::function<void(const drogon::HttpResponsePtr &)> &&cb,
           const std::string &param) {
            Json::Value body;
            body["status"] = "CLOSED";
            body["refund_id"] = "wx_refund_closed";
            body["out_refund_no"] = param;
            auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
            cb(resp);
        },
        {drogon::Get});

    std::thread serverThread([]() { drogon::app().run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    const auto tempDir = std::filesystem::temp_directory_path();
    const auto keyPath =
        tempDir / ("wechatpay_key_" + drogon::utils::getUuid() + ".pem");
    CHECK(writeTempPrivateKey(keyPath));

    Json::Value wechatConfig;
    wechatConfig["api_base"] =
        "http://127.0.0.1:" + std::to_string(port);
    wechatConfig["mch_id"] = "mch_444";
    wechatConfig["serial_no"] = "SERIAL_REFUND_CLOSED";
    wechatConfig["private_key_path"] = keyPath.string();
    wechatConfig["api_v3_key"] = "0123456789abcdef0123456789abcdef";
    auto wechatClient = std::make_shared<WechatPayClient>(wechatConfig);

    PayPlugin plugin;
    plugin.setTestClients(wechatClient, client);

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setParameter("refund_no", refundNo);

    std::promise<drogon::HttpResponsePtr> promise;
    plugin.queryRefund(
        req,
        [&promise](const drogon::HttpResponsePtr &resp) {
            promise.set_value(resp);
        });

    auto future = promise.get_future();
    CHECK(future.wait_for(std::chrono::seconds(5)) ==
          std::future_status::ready);
    const auto resp = future.get();
    CHECK(resp != nullptr);
    CHECK(resp->statusCode() == drogon::k200OK);
    const auto respJson = resp->getJsonObject();
    CHECK(respJson != nullptr);
    CHECK((*respJson)["refund_no"].asString() == refundNo);
    CHECK((*respJson)["status"].asString() == "REFUND_FAIL");
    CHECK((*respJson)["wechat_response"]["status"].asString() == "CLOSED");

    const auto updated =
        refundMapper.findByPrimaryKey(refund.getValueOfId());
    CHECK(updated.getValueOfStatus() == "REFUND_FAIL");
    CHECK(updated.getValueOfChannelRefundNo() == "wx_refund_closed");

    const auto ledgerRows = client->execSqlSync(
        "SELECT COUNT(*) AS cnt FROM pay_ledger WHERE order_no = $1",
        orderNo);
    CHECK(!ledgerRows.empty());
    CHECK(ledgerRows.front()["cnt"].as<int64_t>() == 0);

    drogon::app().quit();
    if (serverThread.joinable())
    {
        serverThread.join();
    }

    std::error_code ec;
    std::filesystem::remove(keyPath, ec);
    client->execSqlSync("DELETE FROM pay_refund WHERE refund_no = $1",
                        refundNo);
    client->execSqlSync("DELETE FROM pay_ledger WHERE order_no = $1", orderNo);
}

DROGON_TEST(PayPlugin_QueryRefund_WechatAbnormal)
{
    Json::Value root;
    CHECK(loadConfig(root));
    CHECK(root.isMember("db_clients"));
    CHECK(root["db_clients"].isArray());
    CHECK(!root["db_clients"].empty());

    const auto &db = root["db_clients"][0];
    const std::string connInfo = buildPgConnInfo(db);
    CHECK(!connInfo.empty());

    auto client = drogon::orm::DbClient::newPgClient(connInfo, 1);
    CHECK(client != nullptr);

    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_refund ("
        "id BIGSERIAL PRIMARY KEY,"
        "refund_no VARCHAR(64) NOT NULL UNIQUE,"
        "order_no VARCHAR(64) NOT NULL,"
        "payment_no VARCHAR(64) NOT NULL,"
        "channel_refund_no VARCHAR(64),"
        "status VARCHAR(24) NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "response_payload TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");
    client->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pay_ledger ("
        "id BIGSERIAL PRIMARY KEY,"
        "user_id BIGINT NOT NULL,"
        "order_no VARCHAR(64) NOT NULL,"
        "payment_no VARCHAR(64),"
        "entry_type VARCHAR(16) NOT NULL,"
        "amount DECIMAL(18,2) NOT NULL,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW())");

    const std::string refundNo = "refund_" + drogon::utils::getUuid();
    const std::string orderNo = "ord_" + drogon::utils::getUuid();
    const std::string paymentNo = "pay_" + drogon::utils::getUuid();
    const std::string amount = "2.58";

    using PayRefund = drogon_model::pay_test::PayRefund;
    drogon::orm::Mapper<PayRefund> refundMapper(client);
    PayRefund refund;
    refund.setRefundNo(refundNo);
    refund.setOrderNo(orderNo);
    refund.setPaymentNo(paymentNo);
    refund.setStatus("REFUNDING");
    refund.setAmount(amount);
    refund.setCreatedAt(trantor::Date::now());
    refund.setUpdatedAt(trantor::Date::now());
    refundMapper.insert(refund);

    const uint16_t port = 24087;
    drogon::app().addListener("127.0.0.1", port);
    drogon::app().registerHandler(
        "/v3/refund/domestic/refunds/{1}",
        [](const drogon::HttpRequestPtr &req,
           std::function<void(const drogon::HttpResponsePtr &)> &&cb,
           const std::string &param) {
            Json::Value body;
            body["status"] = "ABNORMAL";
            body["refund_id"] = "wx_refund_abnormal";
            body["out_refund_no"] = param;
            auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
            cb(resp);
        },
        {drogon::Get});

    std::thread serverThread([]() { drogon::app().run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    const auto tempDir = std::filesystem::temp_directory_path();
    const auto keyPath =
        tempDir / ("wechatpay_key_" + drogon::utils::getUuid() + ".pem");
    CHECK(writeTempPrivateKey(keyPath));

    Json::Value wechatConfig;
    wechatConfig["api_base"] =
        "http://127.0.0.1:" + std::to_string(port);
    wechatConfig["mch_id"] = "mch_445";
    wechatConfig["serial_no"] = "SERIAL_REFUND_ABNORMAL";
    wechatConfig["private_key_path"] = keyPath.string();
    wechatConfig["api_v3_key"] = "0123456789abcdef0123456789abcdef";
    auto wechatClient = std::make_shared<WechatPayClient>(wechatConfig);

    PayPlugin plugin;
    plugin.setTestClients(wechatClient, client);

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setParameter("refund_no", refundNo);

    std::promise<drogon::HttpResponsePtr> promise;
    plugin.queryRefund(
        req,
        [&promise](const drogon::HttpResponsePtr &resp) {
            promise.set_value(resp);
        });

    auto future = promise.get_future();
    CHECK(future.wait_for(std::chrono::seconds(5)) ==
          std::future_status::ready);
    const auto resp = future.get();
    CHECK(resp != nullptr);
    CHECK(resp->statusCode() == drogon::k200OK);
    const auto respJson = resp->getJsonObject();
    CHECK(respJson != nullptr);
    CHECK((*respJson)["refund_no"].asString() == refundNo);
    CHECK((*respJson)["status"].asString() == "REFUND_FAIL");
    CHECK((*respJson)["wechat_response"]["status"].asString() == "ABNORMAL");

    const auto updated =
        refundMapper.findByPrimaryKey(refund.getValueOfId());
    CHECK(updated.getValueOfStatus() == "REFUND_FAIL");
    CHECK(updated.getValueOfChannelRefundNo() == "wx_refund_abnormal");

    const auto ledgerRows = client->execSqlSync(
        "SELECT COUNT(*) AS cnt FROM pay_ledger WHERE order_no = $1",
        orderNo);
    CHECK(!ledgerRows.empty());
    CHECK(ledgerRows.front()["cnt"].as<int64_t>() == 0);

    drogon::app().quit();
    if (serverThread.joinable())
    {
        serverThread.join();
    }

    std::error_code ec;
    std::filesystem::remove(keyPath, ec);
    client->execSqlSync("DELETE FROM pay_refund WHERE refund_no = $1",
                        refundNo);
    client->execSqlSync("DELETE FROM pay_ledger WHERE order_no = $1", orderNo);
}
