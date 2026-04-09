#include "IdempotencyService.h"
#include <drogon/drogon.h>
#include <openssl/sha.h>
#include <sstream>

using namespace drogon;

IdempotencyService::IdempotencyService(
    std::shared_ptr<orm::DbClient> dbClient,
    nosql::RedisClientPtr redisClient,
    int64_t ttlSeconds)
    : dbClient_(dbClient), redisClient_(redisClient), ttlSeconds_(ttlSeconds) {
}

void IdempotencyService::checkAndSet(
    const std::string& idempotencyKey,
    const std::string& requestHash,
    const Json::Value& request,
    CheckCallback&& callback) {

    // Step 1: Check Redis first (fast path)
    std::string redisKey = "idempotency:" + idempotencyKey;
    redisClient_->execCommandAsync(
        [this, idempotencyKey, requestHash, request, callback](const nosql::RedisResult& result) {
            if (!result.isNil()) {
                // Cache hit - return cached result
                try {
                    Json::Value cached = Json::Value();
                    Json::Reader reader;
                    reader.parse(result.asString(), cached);

                    if (cached["request_hash"].asString() == requestHash) {
                        // Same request - return cached response
                        callback(true, cached["response"]);
                    } else {
                        // Different request - idempotency conflict
                        callback(false, Json::Value());
                    }
                } catch (...) {
                    callback(false, Json::Value());
                }
                return;
            }

            // Step 2: Check database
            dbClient_->execSqlAsync(
                "SELECT request_hash, response FROM pay_idempotency WHERE key = $1",
                [this, idempotencyKey, requestHash, request, callback](const orm::Result& rows) {
                    if (!rows.empty()) {
                        std::string cachedHash = rows[0]["request_hash"].c_str();

                        if (cachedHash == requestHash) {
                            // Same request - backfill Redis and return
                            Json::Value response;
                            Json::Reader reader;
                            reader.parse(rows[0]["response"].c_str(), response);

                            // Update Redis cache
                            Json::Value cached;
                            cached["request_hash"] = requestHash;
                            cached["response"] = response;

                            std::string redisKey = "idempotency:" + idempotencyKey;
                            std::string cacheStr = Json::writeString(Json::StreamWriterBuilder(), cached);

                            redisClient_->execCommandAsync(
                                [callback, response](const nosql::RedisResult&) {
                                    callback(true, response);
                                },
                                [](const nosql::RedisException&) {
                                    // Ignore Redis errors - DB is source of truth
                                },
                                "SETEX %s %d %s",
                                redisKey.c_str(),
                                ttlSeconds_,
                                cacheStr.c_str()
                            );
                        } else {
                            // Different request - conflict
                            callback(false, Json::Value());
                        }
                        return;
                    }

                    // Step 3: First request - insert placeholder
                    insertToDatabase(idempotencyKey, requestHash, request, [callback]() {
                        callback(true, Json::Value());
                    });
                },
                [callback](const orm::DrogonDbException& e) {
                    // On database error, fail the idempotency check
                    LOG_ERROR << "Idempotency DB check error: " << e.base().what();
                    callback(false, Json::Value());
                },
                idempotencyKey
            );
        },
        [](const nosql::RedisException&) {
            // On Redis error, fall through to database check
        },
        "GET %s",
        redisKey.c_str()
    );
}

void IdempotencyService::updateResult(
    const std::string& idempotencyKey,
    const Json::Value& response,
    UpdateCallback&& callback) {

    // First, get the request_hash from database to maintain cache structure consistency
    dbClient_->execSqlAsync(
        "SELECT request_hash FROM pay_idempotency WHERE key = $1",
        [this, idempotencyKey, response, callback](const orm::Result& rows) {
            if (rows.empty()) {
                LOG_ERROR << "IdempotencyService::updateResult: Key not found in database: " << idempotencyKey;
                callback();
                return;
            }

            std::string requestHash = rows[0]["request_hash"].c_str();
            std::string responseStr = Json::writeString(Json::StreamWriterBuilder(), response);

            // Update database with response
            dbClient_->execSqlAsync(
                "UPDATE pay_idempotency SET response = $1, updated_at = CURRENT_TIMESTAMP WHERE key = $2",
                [this, idempotencyKey, requestHash, response, callback](const orm::Result& result) {
                    // Update Redis cache with proper structure (including request_hash)
                    Json::Value cached;
                    cached["request_hash"] = requestHash;
                    cached["response"] = response;

                    std::string redisKey = "idempotency:" + idempotencyKey;
                    std::string cacheStr = Json::writeString(Json::StreamWriterBuilder(), cached);

                    redisClient_->execCommandAsync(
                        [callback](const nosql::RedisResult&) {
                            callback();
                        },
                        [callback](const nosql::RedisException&) {
                            // Ignore Redis errors - DB is source of truth
                            callback();
                        },
                        "SETEX %s %d %s",
                        redisKey.c_str(),
                        ttlSeconds_,
                        cacheStr.c_str()
                    );
                },
                [callback](const orm::DrogonDbException& e) {
                    LOG_ERROR << "Idempotency DB update error: " << e.base().what();
                    callback();
                },
                responseStr, idempotencyKey
            );
        },
        [callback](const orm::DrogonDbException& e) {
            LOG_ERROR << "Idempotency DB query error: " << e.base().what();
            callback();
        },
        idempotencyKey
    );
}

void IdempotencyService::insertToDatabase(
    const std::string& key,
    const std::string& hash,
    const Json::Value& request,
    std::function<void()>&& callback) {

    std::string requestStr = Json::writeString(Json::StreamWriterBuilder(), request);

    dbClient_->execSqlAsync(
        "INSERT INTO pay_idempotency (key, request_hash, request) VALUES ($1, $2, $3)",
        [this, key, hash, request, callback](const orm::Result& result) {
            // Backfill Redis
            Json::Value cached;
            cached["request_hash"] = hash;
            cached["request"] = request;
            cached["response"] = Json::Value();

            std::string redisKey = "idempotency:" + key;
            std::string cacheStr = Json::writeString(Json::StreamWriterBuilder(), cached);

            redisClient_->execCommandAsync(
                [callback](const nosql::RedisResult&) {
                    callback();
                },
                [callback](const nosql::RedisException&) {
                    // Ignore Redis errors - DB is source of truth
                    callback();
                },
                "SETEX %s %d %s",
                redisKey.c_str(),
                ttlSeconds_,
                cacheStr.c_str()
            );
        },
        [callback](const orm::DrogonDbException& e) {
            LOG_ERROR << "Idempotency DB insert error: " << e.base().what();
            callback();
        },
        key, hash, requestStr
    );
}
