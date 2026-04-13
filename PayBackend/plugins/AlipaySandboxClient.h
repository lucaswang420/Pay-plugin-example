#pragma once

#include <json/json.h>
#include <functional>
#include <string>
#include <map>
#include <mutex>
#include <memory>

/**
 * @brief Alipay Sandbox Payment Client
 *
 * 支付宝沙箱环境支付客户端，用于开发和测试。
 *
 * 优势：
 * - 个人可注册，无需营业执照
 * - 完全免费
 * - 提供完整 API 支持
 * - 模拟真实支付流程
 *
 * 获取沙箱账号：
 * 1. 访问 https://open.alipay.com/
 * 2. 注册并登录
 * 3. 进入"研发服务" -> "沙箱环境"
 * 4. 获取沙箱 AppID 和密钥
 * 5. 下载沙箱工具生成密钥对
 *
 * 文档：https://opendocs.alipay.com/open/02ivbs
 */
class AlipaySandboxClient
{
  public:
    using JsonCallback =
        std::function<void(const Json::Value &result, const std::string &error)>;

    explicit AlipaySandboxClient(const Json::Value &config);

    /**
     * @brief 创建支付订单（统一下单）
     *
     * @param payload 支付请求参数
     * @param callback 回调函数
     */
    void createTrade(const Json::Value &payload,
                    JsonCallback &&callback);

    /**
     * @brief 查询订单状态
     *
     * @param outTradeNo 商户订单号
     * @param callback 回调函数
     */
    void queryTrade(const std::string &outTradeNo,
                   JsonCallback &&callback);

    /**
     * @brief 创建退款
     *
     * @param payload 退款请求参数
     * @param callback 回调函数
     */
    void refund(const Json::Value &payload,
               JsonCallback &&callback);

    /**
     * @brief 查询退款状态
     *
     * @param outTradeNo 商户订单号
     * @param callback 回调函数
     */
    void queryRefund(const std::string &outTradeNo,
                    JsonCallback &&callback);

    /**
     * @brief 关闭订单
     *
     * @param outTradeNo 商户订单号
     * @param callback 回调函数
     */
    void closeTrade(const std::string &outTradeNo,
                   JsonCallback &&callback);

    /**
     * @brief 验证支付宝回调签名
     *
     * @param params 回调参数
     * @param signature 签名
     * @return 验证是否成功
     */
    bool verifyCallback(const Json::Value &params,
                       const std::string &signature);

    /**
     * @brief 获取沙箱应用信息
     */
    const std::string &getAppId() const { return appId_; }
    const std::string &getSellerId() const { return sellerId_; }

  private:
    Json::Value config_;
    std::string appId_;
    std::string sellerId_;          // 卖家支付宝用户ID（邮箱）
    std::string privateKeyPath_;    // 应用私钥文件路径
    std::string alipayPublicKeyPath_; // 支付宝公钥文件路径
    std::string gatewayUrl_;         // 沙箱网关地址

    // 加载私钥用于签名
    std::string loadPrivateKey() const;

    // 加载支付宝公钥用于验证签名
    std::string loadAlipayPublicKey() const;

    // RSA 签名
    std::string sign(const std::string &data) const;

    // 验证签名
    bool verify(const std::string &data,
               const std::string &signature) const;

    // 构建请求参数
    Json::Value buildCommonParams() const;

    // 发送 HTTP 请求
    void sendRequest(const std::string &method,
                    const Json::Value &bizContent,
                    JsonCallback &&callback);
};
