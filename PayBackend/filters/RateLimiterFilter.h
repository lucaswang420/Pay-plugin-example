#pragma once

#include <drogon/HttpFilter.h>
#include <drogon/nosql/RedisClient.h>
#include <memory>
#include <string>
#include <atomic>
#include <mutex>

/**
 * @brief Rate Limiter Filter
 *
 * Implements token bucket rate limiting using Redis.
 * Prevents API abuse by limiting request rate per API key or IP address.
 *
 * Configuration in config.json:
 * "rate_limit": {
 *   "enabled": true,
 *   "requests_per_minute": 100,
 *   "burst": 10,
 *   "key_type": "api_key",  // or "ip"
 *   "whitelist": ["127.0.0.1"]
 * }
 */
class RateLimiterFilter : public drogon::HttpFilter<RateLimiterFilter>
{
public:
    RateLimiterFilter();
    void doFilter(const drogon::HttpRequestPtr &req,
                  drogon::FilterCallback &&cb,
                  drogon::FilterChainCallback &&ccb) override;

private:
    void initialize();

    drogon::nosql::RedisClientPtr redisClient_;
    std::atomic<bool> initialized_{false};
    std::mutex initMutex_;

    // Rate limit settings
    int requestsPerMinute_ = 100;  // Default: 100 requests per minute
    int burst_ = 10;                // Default: burst capacity
    std::string keyType_ = "api_key"; // "api_key" or "ip"
    bool enabled_ = true;

    // Whitelist
    std::vector<std::string> whitelist_;

    /**
     * Get rate limit key from request
     * Returns API key or IP address based on configuration
     */
    std::string getRateLimitKey(const drogon::HttpRequestPtr &req);

    /**
     * Check if request is allowed under rate limit
     * Uses token bucket algorithm with Redis
     * @return true if request is allowed, false otherwise
     */
    bool isAllowed(const std::string &key);

    /**
     * Check if IP is in whitelist
     */
    bool isWhitelisted(const std::string &ip) const;

    /**
     * Extract IP address from request
     * Handles X-Forwarded-For and X-Real-IP headers
     */
    std::string getClientIP(const drogon::HttpRequestPtr &req);
};
