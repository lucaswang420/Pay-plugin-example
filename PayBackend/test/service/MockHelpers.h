#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <drogon/orm/DbClient.h>
#include <drogon/nosql/RedisClient.h>
#include "../../services/IdempotencyService.h"
#include <memory>
#include <functional>

// Mock DbClient (simplified - in reality use Drogon's test utilities)
class MockDbClient : public drogon::orm::DbClient {
public:
    MockDbClient() = default;

    MOCK_METHOD(
        void,
        execSqlAsync,
        (const std::string& sql,
         std::function<void(const drogon::orm::Result&)>&& callback,
         std::function<void(const drogon::orm::DrogonDbException&)>&& exceptCallback),
        (override)
    );
};

// Mock RedisClient
class MockRedisClient : public drogon::nosql::RedisClient {
public:
    MockRedisClient() = default;

    MOCK_METHOD(
        void,
        execCommandAsync,
        (const std::string& command,
         std::function<void(const drogon::nosql::RedisResult&)>&& callback,
         std::function<void(const drogon::nosql::RedisException&)>&& exceptCallback),
        ()
    );
};

// Mock IdempotencyService
class MockIdempotencyService : public IdempotencyService {
public:
    MockIdempotencyService(
        std::shared_ptr<drogon::orm::DbClient> dbClient,
        drogon::nosql::RedisClientPtr redisClient,
        int64_t ttlSeconds = 604800)
        : IdempotencyService(dbClient, redisClient, ttlSeconds) {}

    MOCK_METHOD(
        void,
        checkAndSet,
        (const std::string& idempotencyKey,
         const std::string& requestHash,
         const Json::Value& request,
         CheckCallback&& callback),
        (override)
    );

    MOCK_METHOD(
        void,
        updateResult,
        (const std::string& idempotencyKey,
         const Json::Value& response,
         UpdateCallback&& callback),
        (override)
    );
};
