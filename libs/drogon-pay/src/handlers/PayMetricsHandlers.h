#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <functional>

using namespace drogon;

/// Plain handler class held by PayPlugin; routes {base_path}/metrics/auth and
/// {base_path}/metrics/auth.prom are registered programmatically in
/// PayPlugin::initAndStart (see PayController.h for the rationale).
class PayMetricsController
{
  public:
    void authMetrics(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );
    void authMetricsProm(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );
};
