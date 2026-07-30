#include "AuthCheck.h"
#include "PayAuthMetrics.h"
#include <drogon/drogon.h>
#include <openssl/crypto.h>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

namespace
{
// Constant-time string comparison to prevent timing attacks on API key
// validation. std::string::operator== (used by std::find) short-circuits on
// the first differing byte, allowing an attacker to recover a valid key
// byte-by-byte by measuring response time (P1-2). Length is compared first;
// only equal-length comparisons go through CRYPTO_memcmp (length itself is
// not secret in practice — only the content must be compared in constant time).
bool constantTimeEquals(const std::string &a, const std::string &b)
{
    if (a.size() != b.size())
    {
        return false;
    }
    return CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
}

// Strict constant-time key lookup: scans ALL keys without early return so an
// attacker cannot infer the number of valid API keys via timing (S-1).
bool containsKeyConstantTime(const std::vector<std::string> &keys, const std::string &key)
{
    bool found = false;
    for (const auto &allowed : keys)
    {
        if (constantTimeEquals(allowed, key))
        {
            found = true;
        }
    }
    return found;
}

std::string trim(const std::string &value)
{
    const auto start = value.find_first_not_of(" \t");
    if (start == std::string::npos)
    {
        return {};
    }
    const auto end = value.find_last_not_of(" \t");
    return value.substr(start, end - start + 1);
}

std::vector<std::string> splitKeys(const std::string &value)
{
    std::vector<std::string> keys;
    std::stringstream ss(value);
    std::string token;
    while (std::getline(ss, token, ','))
    {
        auto key = trim(token);
        if (!key.empty())
        {
            keys.push_back(key);
        }
    }
    return keys;
}

std::string extractApiKey(const drogon::HttpRequestPtr &req)
{
    auto key = req->getHeader("X-Api-Key");
    if (!key.empty())
    {
        return key;
    }

    const auto auth = req->getHeader("Authorization");
    const std::string bearer = "Bearer ";
    if (auth.rfind(bearer, 0) == 0 && auth.size() > bearer.size())
    {
        return auth.substr(bearer.size());
    }
    return auth;
}

// Scope is resolved from the path suffix relative to the configured base
// path, so scope enforcement keeps working when hosts change `base_path`.
std::string resolveScope(const drogon::HttpRequestPtr &req, const std::string &basePath)
{
    const auto &path = req->path();
    if (path.rfind(basePath, 0) != 0)
    {
        return {};
    }
    const std::string rel = path.substr(basePath.size());
    if (rel.rfind("/refund/query", 0) == 0)
    {
        return "refund_query";
    }
    if (rel.rfind("/refund", 0) == 0)
    {
        return "refund";
    }
    if (rel.rfind("/query", 0) == 0)
    {
        return "order_query";
    }
    if (rel.rfind("/reconcile", 0) == 0)
    {
        return "reconcile";
    }
    return {};
}
}  // namespace

namespace drogon_pay
{

drogon::HttpResponsePtr checkAuth(const drogon::HttpRequestPtr &req, const std::string &basePath)
{
    if (req->method() == drogon::Options)
    {
        return nullptr;  // CORS preflight passes through
    }

    std::vector<std::string> allowedKeys;
    const auto &customConfig = drogon::app().getCustomConfig();
    if (
      customConfig.isMember("pay") && customConfig["pay"].isMember("api_keys") &&
      customConfig["pay"]["api_keys"].isArray()
    )
    {
        for (const auto &item : customConfig["pay"]["api_keys"])
        {
            auto key = item.asString();
            if (!key.empty())
            {
                allowedKeys.push_back(key);
            }
        }
    }

    if (const char *singleKey = std::getenv("PAY_API_KEY"))
    {
        auto key = trim(singleKey);
        if (!key.empty())
        {
            allowedKeys.push_back(key);
        }
    }

    if (const char *multiKeys = std::getenv("PAY_API_KEYS"))
    {
        const auto extra = splitKeys(multiKeys);
        allowedKeys.insert(allowedKeys.end(), extra.begin(), extra.end());
    }

    if (allowedKeys.empty())
    {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k503ServiceUnavailable);
        resp->setBody("api key not configured");
        LOG_WARN << "checkAuth: api key not configured";
        PayAuthMetrics::incNotConfigured();
        return resp;
    }

    const auto key = extractApiKey(req);
    if (key.empty())
    {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k401Unauthorized);
        resp->setBody("missing api key");
        LOG_WARN << "checkAuth: missing api key";
        PayAuthMetrics::incMissingKey();
        return resp;
    }

    const auto match = containsKeyConstantTime(allowedKeys, key);
    if (!match)
    {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k401Unauthorized);
        resp->setBody("invalid api key");
        // Never log key material (the old filter leaked request/allowed keys
        // into WARN logs); counts are enough for diagnostics.
        LOG_WARN << "checkAuth: invalid api key, allowed_count=" << allowedKeys.size();
        PayAuthMetrics::incInvalidKey();
        return resp;
    }

    const auto scope = resolveScope(req, basePath);
    if (
      !scope.empty() && customConfig.isMember("pay") &&
      customConfig["pay"].isMember("api_key_scopes") &&
      customConfig["pay"]["api_key_scopes"].isObject()
    )
    {
        const auto &scopeConfig = customConfig["pay"]["api_key_scopes"];
        if (scopeConfig.isMember(key))
        {
            const auto &scopes = scopeConfig[key];
            bool allowed = false;
            if (scopes.isArray())
            {
                for (const auto &item : scopes)
                {
                    if (item.asString() == scope)
                    {
                        allowed = true;
                        break;
                    }
                }
            }
            if (!allowed)
            {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k403Forbidden);
                resp->setBody("api key scope not allowed");
                LOG_WARN << "checkAuth: key scope not allowed";
                PayAuthMetrics::incScopeDenied();
                return resp;
            }
        }
        else if (customConfig["pay"].isMember("api_key_default_scopes"))
        {
            const auto &defaults = customConfig["pay"]["api_key_default_scopes"];
            bool allowed = false;
            if (defaults.isArray())
            {
                for (const auto &item : defaults)
                {
                    if (item.asString() == scope)
                    {
                        allowed = true;
                        break;
                    }
                }
            }
            if (!allowed)
            {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k403Forbidden);
                resp->setBody("api key scope not allowed");
                LOG_WARN << "checkAuth: default scope not allowed";
                PayAuthMetrics::incScopeDenied();
                return resp;
            }
        }
    }

    return nullptr;
}

}  // namespace drogon_pay
