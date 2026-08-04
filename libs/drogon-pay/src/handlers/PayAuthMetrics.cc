#include "PayAuthMetrics.h"
#include <string>

std::atomic<uint64_t> PayAuthMetrics::missingKey_{0};
std::atomic<uint64_t> PayAuthMetrics::invalidKey_{0};
std::atomic<uint64_t> PayAuthMetrics::scopeDenied_{0};
std::atomic<uint64_t> PayAuthMetrics::notConfigured_{0};

void PayAuthMetrics::incMissingKey()
{
    ++missingKey_;
}

void PayAuthMetrics::incInvalidKey()
{
    ++invalidKey_;
}

void PayAuthMetrics::incScopeDenied()
{
    ++scopeDenied_;
}

void PayAuthMetrics::incNotConfigured()
{
    ++notConfigured_;
}

Json::Value PayAuthMetrics::snapshot()
{
    Json::Value root;
    root["missing_key"] = static_cast<Json::UInt64>(missingKey_.load());
    root["invalid_key"] = static_cast<Json::UInt64>(invalidKey_.load());
    root["scope_denied"] = static_cast<Json::UInt64>(scopeDenied_.load());
    root["not_configured"] = static_cast<Json::UInt64>(notConfigured_.load());
    return root;
}

std::string PayAuthMetrics::toPrometheus()
{
    const auto s = snapshot();
    std::string body;

    auto emit = [&body](const std::string &name, const std::string &help, Json::UInt64 val) {
        body += "# HELP " + name + " " + help + "\n";
        body += "# TYPE " + name + " counter\n";
        body += name + " " + std::to_string(val) + "\n";
    };

    emit("pay_auth_missing_key_total", "Missing API key count", s["missing_key"].asUInt64());
    emit("pay_auth_invalid_key_total", "Invalid API key count", s["invalid_key"].asUInt64());
    emit("pay_auth_scope_denied_total", "Scope denied count", s["scope_denied"].asUInt64());
    emit("pay_auth_not_configured_total", "Not configured count", s["not_configured"].asUInt64());

    return body;
}
