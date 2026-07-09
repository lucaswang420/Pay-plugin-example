#pragma once
#include <cstdint>
#include <json/json.h>
#include <string>

namespace pay::utils
{
bool getRequiredString(const Json::Value &json, const char *key, std::string &value);

bool parseAmountToFen(const std::string &amount, int64_t &fen);

std::string toJsonString(const Json::Value &value);

void mapTradeState(
  const std::string &tradeState,
  std::string &orderStatus,
  std::string &paymentStatus
);

std::string mapRefundStatus(const std::string &wechatStatus);

// Validate a callback notify URL. Returns true if the URL is empty (no notify
// URL supplied is allowed) or passes scheme + length checks. On failure, sets
// errorMessage. Used by both PaymentService (create) and RefundService to
// guard against SSRF via an attacker-controlled notify_url (P1-3).
bool validateNotifyUrl(const std::string &url, std::string &errorMessage);
}  // namespace pay::utils
