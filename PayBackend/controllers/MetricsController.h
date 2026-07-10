#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

class MetricsController : public drogon::HttpController<MetricsController>
{
  public:
    METHOD_LIST_BEGIN
    // Restrict /metrics to loopback (P2). Prometheus typically runs as a
    // same-host sidecar; external callers must not read internal counters.
    // Deployments that scrape remotely should put a reverse proxy with auth in
    // front rather than expose this endpoint publicly.
    ADD_METHOD_TO(MetricsController::metrics, "/metrics", Get, Options, "drogon::LocalHostFilter");
    METHOD_LIST_END

    void metrics(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );
};
