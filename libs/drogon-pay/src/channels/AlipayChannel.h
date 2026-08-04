#pragma once

#include "drogon_pay/PaymentChannel.h"

#include <json/json.h>
#include <functional>
#include <string>
#include <map>
#include <mutex>
#include <memory>

/**
 * @brief Alipay Sandbox Payment Client
 *
 * Alipay sandbox environment payment client for development and testing.
 *
 * Advantages:
 * - Individual registration available, no business license required
 * - Completely free
 * - Full API support
 * - Simulates real payment workflow
 *
 * Getting sandbox account:
 * 1. Visit https://open.alipay.com/
 * 2. Register and login
 * 3. Go to "Development Service" -> "Sandbox Environment"
 * 4. Get sandbox AppID and keys
 * 5. Download sandbox tools to generate key pairs
 *
 * Documentation: https://opendocs.alipay.com/open/02ivbs
 */
class AlipaySandboxClient : public drogon_pay::PaymentChannel
{
  public:
    using JsonCallback = std::function<void(const Json::Value &result, const std::string &error)>;

    explicit AlipaySandboxClient(const Json::Value &config);

    // ---- PaymentChannel SPI ----
    const std::string &name() const override;
    bool isConfigured() const override;
    /// App/wap payment -> alipay.trade.create
    void createPayment(const Json::Value &payload, JsonCallback &&callback) override;
    /// QR payment -> alipay.trade.precreate
    void createQRPayment(const Json::Value &payload, JsonCallback &&callback) override;
    /// Query -> alipay.trade.query
    void queryPayment(const std::string &orderNo, JsonCallback &&callback) override;
    void refund(const Json::Value &payload, JsonCallback &&callback) override;
    void queryRefund(const std::string &outTradeNo, JsonCallback &&callback) override;
    /// Parses the x-www-form-urlencoded notification body, verifies the RSA2
    /// signature and normalizes the fields into a CallbackEvent.
    bool verifyCallback(
      const drogon::HttpRequestPtr &req,
      drogon_pay::CallbackEvent &event,
      std::string &error
    ) override;

    // ---- Alipay-specific capabilities (reach via dynamic_pointer_cast) ----

    /**
     * @brief Create payment order
     *
     * @param payload Payment request parameters
     * @param callback Callback function
     */
    void createTrade(const Json::Value &payload, JsonCallback &&callback);

    /**
     * @brief Precreate payment order (QR code payment)
     *
     * @param payload Payment request parameters
     * @param callback Callback function
     */
    void precreateTrade(const Json::Value &payload, JsonCallback &&callback);

    /**
     * @brief Query order status
     *
     * @param outTradeNo Merchant order number
     * @param callback Callback function
     */
    void queryTrade(const std::string &outTradeNo, JsonCallback &&callback);

    /**
     * @brief Close order
     *
     * @param outTradeNo Merchant order number
     * @param callback Callback function
     */
    void closeTrade(const std::string &outTradeNo, JsonCallback &&callback);

    /**
     * @brief Verify Alipay callback signature
     *
     * @param params Callback parameters
     * @param signature Signature
     * @return Verification success
     */
    bool verifyCallback(const Json::Value &params, const std::string &signature);

    /**
     * @brief Get sandbox application information
     */
    const std::string &getAppId() const
    {
        return appId_;
    }

    const std::string &getSellerId() const
    {
        return sellerId_;
    }

    /**
     * @brief Generate unique ID
     */
    std::string generateUUID() const;

  private:
    Json::Value config_;
    std::string appId_;
    std::string sellerId_;             // Seller Alipay user ID (email)
    std::string privateKeyPath_;       // Application private key file path
    std::string alipayPublicKeyPath_;  // Alipay public key file path
    std::string gatewayUrl_;           // Sandbox gateway URL
    std::string notifyUrl_;            // Async callback URL
    int timeoutMs_;                    // HTTP request timeout in milliseconds

    // PEM contents loaded once at construction (B3: previously the key files
    // were re-read and re-parsed on every request, costing disk I/O per call
    // and exposing a TOCTOU window where the file could be swapped between the
    // stat and the read).
    std::string privateKeyPem_;
    std::string alipayPublicKeyPem_;

    // RSA signing
    std::string sign(const std::string &data) const;

    // Verify signature
    bool verify(const std::string &data, const std::string &signature) const;

    // Build request parameters
    Json::Value buildCommonParams() const;

    // Send HTTP request
    void sendRequest(
      const std::string &method,
      const Json::Value &bizContent,
      JsonCallback &&callback
    );
};
