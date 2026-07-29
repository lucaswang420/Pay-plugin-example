#include "PayMetricsController.h"
#include "../filters/PayAuthMetrics.h"

void PayMetricsController::authMetrics(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    if (req->method() == Options)
    {
        auto resp = HttpResponse::newHttpResponse();
        callback(resp);
        return;
    }

    auto body = PayAuthMetrics::snapshot();
    auto resp = HttpResponse::newHttpJsonResponse(body);
    callback(resp);
}

void PayMetricsController::authMetricsProm(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    if (req->method() == Options)
    {
        auto resp = HttpResponse::newHttpResponse();
        callback(resp);
        return;
    }

    // D2-1: use shared Prometheus serializer instead of duplicating format logic
    std::string body = PayAuthMetrics::toPrometheus();

    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    resp->setContentTypeCode(CT_TEXT_PLAIN);
    resp->setBody(body);
    callback(resp);
}
