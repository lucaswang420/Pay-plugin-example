#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <string>

namespace drogon_pay
{

/// Programmatic replacement for the old PayAuthFilter drogon filter.
/// Handlers call this before doing any work: a null return means the request
/// is authorized; a non-null return is the error response (401/403/503) the
/// handler must send back immediately.
///
/// Filter registration was dropped on purpose: drogon filter self-registration
/// relies on static-init symbols that linkers drop from static libraries.
/// `basePath` is the configured route prefix (used for scope resolution).
drogon::HttpResponsePtr checkAuth(const drogon::HttpRequestPtr &req, const std::string &basePath);

}  // namespace drogon_pay
