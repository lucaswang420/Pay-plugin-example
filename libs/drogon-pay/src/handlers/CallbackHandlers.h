#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <functional>

using namespace drogon;

/// Plain handler classes held by PayPlugin; routes {base_path}/notify/wechat
/// and {base_path}/notify/alipay are registered programmatically in
/// PayPlugin::initAndStart (see PayHandlers.h for the rationale).
class WechatCallbackController
{
  public:
    void notify(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);
};

class AlipayCallbackController
{
  public:
    void notify(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);
};
