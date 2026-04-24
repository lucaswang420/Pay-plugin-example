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

    // Wrap callback in shared_ptr to prevent it from being destroyed during async operations
    auto sharedCb = std::make_shared<CheckCallback>(std::move(callback));

    // Step 1: Check Redis first (fast path) - only if Redis client is available
    if (redisClient_) {
        std::string redisKey = "idempotency:" + idempotencyKey;
        redisClient_->execCommandAsync(
            [this, idempotencyKey, requestHash, request, sharedCb](const nosql::RedisResult& result) {
                if (!result.isNil()) {
                    // Cache hit - return cached result
                    try {
                        Json::Value cached = Json::Value();
                        Json::CharReaderBuilder builder;
                        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
                        std::string errors;
                        const char* str = result.asString().c_str();
                        reader->parse(str, str + result.asString().length(), &cached, &errors);

                        if (*sharedCb) {
                            if (cached["request_hash"].asString() == requestHash) {
                                // Same request - return cached response
                                (*sharedCb)(true, cached["response"]);
                            } else {
                                // Different request - idempotency conflict
                                (*sharedCb)(false, Json::Value());
                            }
                        }
                    } catch (...) {
                        if (*sharedCb) {
                            (*sharedCb)(false, Json::Value());
                        }
                    }
                    return;
                }

                // Cache miss - check database
                checkDatabase(idempotencyKey, requestHash, request, std::move(*sharedCb));
            },
            [this, idempotencyKey, requestHash, request, sharedCb](const std::exception& e) {
                // Redis error - fall back to database
                LOG_WARN << "Redis idempotency check failed: " << e.what();
                checkDatabase(idempotencyKey, requestHash, request, std::move(*sharedCb));
            },
            "GET %s",
            redisKey.c_str()
        );
    } else {
        // Redis not available - check database directly
        checkDatabase(idempotencyKey, requestHash, request, std::move(*sharedCb));
    }
}

void IdempotencyService::checkDatabase(
    const std::string& idempotencyKey,
    const std::string& requestHash,
    const Json::Value& request,
    CheckCallback&& callback) {

    // Wrap callback in shared_ptr to prevent it from being destroyed during async operations
    auto sharedCb = std::make_shared<CheckCallback>(std::move(callback));

    // Step 2: Check database
    dbClient_->execSqlAsync(
        "SELECT request_hash, response_snapshot FROM pay_idempotency WHERE idempotency_key = $1",
        [this, idempotencyKey, requestHash, request, sharedCb](const orm::Result& rows) {
            if (!rows.empty()) {
                std::string cachedHash = rows[0]["request_hash"].c_str();

                if (cachedHash == requestHash) {
                    // Same request - backfill Redis and return
                    Json::Value response;
                    Json::CharReaderBuilder builder;
                    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
                    std::string errors;
                    const char* str = rows[0]["response_snapshot"].c_str();
                    reader->parse(str, str + strlen(str), &response, &errors);

                    // Update Redis cache if available
                    if (redisClient_) {
                        Json::Value cached;
                        cached["request_hash"] = requestHash;
                        cached["response"] = response;

                        std::string redisKey = "idempotency:" + idempotencyKey;
                        std::string cacheStr = Json::writeString(Json::StreamWriterBuilder(), cached);

                        redisClient_->execCommandAsync(
                            [sharedCb, response](const nosql::RedisResult&) {
                                if (*sharedCb) {
                                    (*sharedCb)(true, response);
                                }
                            },
                            [sharedCb, response](const nosql::RedisException&) {
                                // Ignore Redis errors - DB is source of truth
                                if (*sharedCb) {
                                    (*sharedCb)(true, response);
                                }
                            },
                            "SETEX %s %d %s",
                            redisKey.c_str(),
                            ttlSeconds_,
                            cacheStr.c_str()
                        );
                    } else {
                        if (*sharedCb) {
                            (*sharedCb)(true, response);
                        }
                    }
                } else {
                    // Different request - conflict
                    if (*sharedCb) {
                        (*sharedCb)(false, Json::Value());
                    }
                }
                return;
            }

            // Step 3: First request - insert placeholder
            insertToDatabase(idempotencyKey, requestHash, request, [sharedCb]() {
                if (*sharedCb) {
                    (*sharedCb)(true, Json::Value());
                }
            });
        },
        [sharedCb](const orm::DrogonDbException& e) {
            // On database error, fail the idempotency check
            LOG_ERROR << "Idempotency DB check error: " << e.base().what();
            if (*sharedCb) {
                (*sharedCb)(false, Json::Value());
            }
        },
        idempotencyKey
    );
}

void IdempotencyService::updateResult(
    const std::string& idempotencyKey,
    const Json::Value& response,
    UpdateCallback&& callback) {

    // Wrap callback in shared_ptr to prevent it from being destroyed during async operations
    auto sharedCb = std::make_shared<UpdateCallback>(std::move(callback));

    // First, get the request_hash from database to maintain cache structure consistency
    dbClient_->execSqlAsync(
        "SELECT request_hash FROM pay_idempotency WHERE idempotency_key = $1",
        [this, idempotencyKey, response, sharedCb](const orm::Result& rows) {
            if (rows.empty()) {
                LOG_ERROR << "IdempotencyService::updateResult: Key not found in database: " << idempotencyKey;
                if (*sharedCb) {
                    (*sharedCb)();
                }
                return;
            }

            std::string requestHash = rows[0]["request_hash"].c_str();
            std::string responseStr = Json::writeString(Json::StreamWriterBuilder(), response);

            // Update database with response
            dbClient_->execSqlAsync(
                "UPDATE pay_idempotency SET response_snapshot = $1, updated_at = CURRENT_TIMESTAMP WHERE idempotency_key = $2",
                [this, idempotencyKey, requestHash, response, sharedCb](const orm::Result& result) {
                    // Update Redis cache with proper structure (including request_hash) if available
                    if (redisClient_) {
                        Json::Value cached;
                        cached["request_hash"] = requestHash;
                        cached["response"] = response;

                        std::string redisKey = "idempotency:" + idempotencyKey;
                        std::string cacheStr = Json::writeString(Json::StreamWriterBuilder(), cached);

                        redisClient_->execCommandAsync(
                            [sharedCb](const nosql::RedisResult&) {
                                if (*sharedCb) {
                                    (*sharedCb)();
                                }
                            },
                            [sharedCb](const nosql::RedisException&) {
                                // Ignore Redis errors - DB is source of truth
                                if (*sharedCb) {
                                    (*sharedCb)();
                                }
                            },
                            "SETEX %s %d %s",
                            redisKey.c_str(),
                            ttlSeconds_,
                            cacheStr.c_str()
                        );
                    } else {
                        if (*sharedCb) {
                            (*sharedCb)();
                        }
                    }
                },
                [sharedCb](const orm::DrogonDbException& e) {
                    LOG_ERROR << "Idempotency DB update error: " << e.base().what();
                    if (*sharedCb) {
                        (*sharedCb)();
                    }
                },
                responseStr, idempotencyKey
            );
        },
        [sharedCb](const orm::DrogonDbException& e) {
            LOG_ERROR << "Idempotency DB query error: " << e.base().what();
            if (*sharedCb) {
                (*sharedCb)();
            }
        },
        idempotencyKey
    );
}

void IdempotencyService::insertToDatabase(
    const std::string& key,
    const std::string& hash,
    const Json::Value& request,
    std::function<void()>&& callback) {

    // Wrap callback in shared_ptr to prevent it from being destroyed during async operations
    auto sharedCb = std::make_shared<std::function<void()>>(std::move(callback));

    // Build cached response structure (empty response initially)
    Json::Value cachedResponse;
    cachedResponse["request_hash"] = hash;
    cachedResponse["request"] = request;
    cachedResponse["response"] = Json::Value();
    std::string responseStr = Json::writeString(Json::StreamWriterBuilder(), cachedResponse);

    dbClient_->execSqlAsync(
        "INSERT INTO pay_idempotency (idempotency_key, request_hash, response_snapshot) VALUES ($1, $2, $3)",
        [this, key, hash, request, sharedCb](const orm::Result& result) {
            // Backfill Redis if available
            if (redisClient_) {
                Json::Value cached;
                cached["request_hash"] = hash;
                cached["request"] = request;
                cached["response"] = Json::Value();

                std::string redisKey = "idempotency:" + key;
                std::string cacheStr = Json::writeString(Json::StreamWriterBuilder(), cached);

                redisClient_->execCommandAsync(
                    [sharedCb](const nosql::RedisResult&) {
                        if (*sharedCb) {
                            (*sharedCb)();
                        }
                    },
                    [sharedCb](const nosql::RedisException&) {
                        // Ignore Redis errors - DB is source of truth
                        if (*sharedCb) {
                            (*sharedCb)();
                        }
                    },
                    "SETEX %s %d %s",
                    redisKey.c_str(),
                    ttlSeconds_,
                    cacheStr.c_str()
                );
            } else {
                if (*sharedCb) {
                    (*sharedCb)();
                }
            }
        },
        [sharedCb](const orm::DrogonDbException& e) {
            LOG_ERROR << "Idempotency DB insert error: " << e.base().what();
            if (*sharedCb) {
                (*sharedCb)();
            }
        },
        key, hash, responseStr
    );
}
