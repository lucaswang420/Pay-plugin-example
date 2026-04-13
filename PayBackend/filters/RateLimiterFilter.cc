#include "RateLimiterFilter.h"
#include <drogon/HttpResponse.h>
#include <drogon/HttpAppFramework.h>
#include <json/json.h>
#include <trantor/utils/Logger.h>
#include <sstream>

RateLimiterFilter::RateLimiterFilter()
{
    // Configuration will be loaded lazily in initialize()
}

void RateLimiterFilter::initialize()
{
    if (initialized_.load()) {
        return;
    }

    std::lock_guard<std::mutex> lock(initMutex_);

    // Double-check
    if (initialized_.load()) {
        return;
    }

    // Get Redis client from Drogon app
    try {
        redisClient_ = drogon::app().getRedisClient();
        if (!redisClient_) {
            LOG_ERROR << "RateLimiterFilter: Redis client not available";
        }
    } catch (const std::exception &e) {
        LOG_ERROR << "RateLimiterFilter: Failed to get Redis client: " << e.what();
    }

    // Load configuration from custom config
    const auto &config = drogon::app().getCustomConfig();

    // Rate limit settings
    if (config.isMember("rate_limit")) {
        const auto &rateLimitConfig = config["rate_limit"];

        if (rateLimitConfig.isMember("enabled")) {
            enabled_ = rateLimitConfig["enabled"].asBool();
            if (!enabled_) {
                LOG_INFO << "RateLimiterFilter: Rate limiting is disabled";
                initialized_.store(true);
                return;
            }
        }

        if (rateLimitConfig.isMember("requests_per_minute")) {
            requestsPerMinute_ = rateLimitConfig["requests_per_minute"].asInt();
            LOG_INFO << "RateLimiterFilter: requests_per_minute = " << requestsPerMinute_;
        }

        if (rateLimitConfig.isMember("burst")) {
            burst_ = rateLimitConfig["burst"].asInt();
            LOG_INFO << "RateLimiterFilter: burst = " << burst_;
        }

        if (rateLimitConfig.isMember("key_type")) {
            keyType_ = rateLimitConfig["key_type"].asString();
            LOG_INFO << "RateLimiterFilter: key_type = " << keyType_;
        }

        if (rateLimitConfig.isMember("whitelist")) {
            const auto &whitelist = rateLimitConfig["whitelist"];
            for (const auto &item : whitelist) {
                whitelist_.push_back(item.asString());
            }
            LOG_INFO << "RateLimiterFilter: whitelist size = " << whitelist_.size();
        }
    }

    LOG_INFO << "RateLimiterFilter: Initialized with " << requestsPerMinute_
             << " requests/min, burst " << burst_ << ", key type: " << keyType_;

    initialized_.store(true);
}

void RateLimiterFilter::doFilter(const drogon::HttpRequestPtr &req,
                                 drogon::FilterCallback &&cb,
                                 drogon::FilterChainCallback &&ccb)
{
    // Lazy initialization
    if (!initialized_.load()) {
        initialize();
    }

    // Check if rate limiting is enabled
    if (!enabled_) {
        ccb();
        return;
    }

    // Check if Redis is available
    if (!redisClient_) {
        // If Redis is not available, allow the request but log a warning
        LOG_WARN << "RateLimiterFilter: Redis not available, allowing request";
        ccb();
        return;
    }

    // Get rate limit key
    std::string key = getRateLimitKey(req);

    // Check whitelist
    if (isWhitelisted(key)) {
        LOG_DEBUG << "RateLimiterFilter: Whitelisted key: " << key;
        ccb();
        return;
    }

    // Check rate limit
    if (isAllowed(key)) {
        LOG_DEBUG << "RateLimiterFilter: Request allowed for key: " << key;
        ccb();
    } else {
        LOG_WARN << "RateLimiterFilter: Rate limit exceeded for key: " << key;

        Json::Value json;
        json["result"] = "error";
        json["message"] = "Rate limit exceeded. Please try again later.";
        json["error_code"] = 429;
        json["retry_after"] = 60;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
        resp->setStatusCode(drogon::k429TooManyRequests);
        resp->addHeader("Retry-After", "60");
        resp->addHeader("X-RateLimit-Limit", std::to_string(requestsPerMinute_));
        resp->addHeader("X-RateLimit-Remaining", "0");

        cb(resp);
    }
}

std::string RateLimiterFilter::getRateLimitKey(const drogon::HttpRequestPtr &req)
{
    if (keyType_ == "api_key") {
        // Use API key from X-Api-Key header
        std::string apiKey = req->getHeader("X-Api-Key");
        if (!apiKey.empty()) {
            return "ratelimit:apikey:" + apiKey;
        }
        // Fallback to IP if no API key
        return "ratelimit:ip:" + getClientIP(req);
    } else {
        // Use IP address
        return "ratelimit:ip:" + getClientIP(req);
    }
}

bool RateLimiterFilter::isAllowed(const std::string &key)
{
    // Token bucket algorithm using Redis
    // Key format: ratelimit:apikey:<key>
    // We use Redis INCR and EXPIRE to implement sliding window

    try {
        // Create a time-windowed key (current minute)
        time_t now = time(nullptr);
        std::string windowKey = key + ":window:" + std::to_string(now / 60);

        // Increment counter
        auto result = redisClient_->execCommandSync<long long>(
            [](const drogon::nosql::RedisResult &r) {
                return r.asInteger();
            },
            "INCR %s",
            windowKey.c_str()
        );

        int count = static_cast<int>(result);

        // Set expiration on first request
        if (count == 1) {
            redisClient_->execCommandSync<int>(
                [](const drogon::nosql::RedisResult &r) {
                    return static_cast<int>(r.asInteger());
                },
                "EXPIRE %s 120",
                windowKey.c_str()  // 2 minutes to handle clock skew
            );
        }

        // Check if rate limit exceeded
        bool allowed = (count <= requestsPerMinute_);

        LOG_DEBUG << "RateLimiterFilter: key=" << key
                  << ", count=" << count
                  << ", limit=" << requestsPerMinute_
                  << ", allowed=" << allowed;

        return allowed;

    } catch (const std::exception &e) {
        LOG_ERROR << "RateLimiterFilter: Exception in isAllowed: " << e.what();
        return true; // Allow on error
    }
}

bool RateLimiterFilter::isWhitelisted(const std::string &key) const
{
    // Check if key is in whitelist
    for (const auto &whitelisted : whitelist_) {
        if (key.find(whitelisted) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::string RateLimiterFilter::getClientIP(const drogon::HttpRequestPtr &req)
{
    // Check X-Forwarded-For header (proxy/load balancer)
    std::string forwardedFor = req->getHeader("X-Forwarded-For");
    if (!forwardedFor.empty()) {
        // Take the first IP from the list
        size_t commaPos = forwardedFor.find(',');
        if (commaPos != std::string::npos) {
            return forwardedFor.substr(0, commaPos);
        }
        return forwardedFor;
    }

    // Check X-Real-IP header
    std::string realIP = req->getHeader("X-Real-IP");
    if (!realIP.empty()) {
        return realIP;
    }

    // Fallback to peer address
    return req->getPeerAddr().toIpPort();
}
