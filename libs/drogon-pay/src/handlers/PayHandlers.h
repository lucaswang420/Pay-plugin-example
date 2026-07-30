#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <functional>

using namespace drogon;

/// Plain handler class held by PayPlugin. Routes are registered
/// programmatically in PayPlugin::initAndStart (registerHandler) instead of
/// ADD_METHOD_TO macros: static-library builds drop the self-registration
/// symbols those macros rely on, and programmatic registration also lets the
/// configured base_path take effect.
class PayController
{
  public:
    void createPayment(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );

    void createQRPayment(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );

    void queryOrder(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );

    void refund(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);

    void queryRefund(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );

    void queryOrderList(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );

    void reconcileSummary(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );
};
