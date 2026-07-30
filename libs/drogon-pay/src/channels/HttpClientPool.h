#pragma once

#include <drogon/HttpAppFramework.h>
#include <drogon/HttpClient.h>
#include <trantor/net/EventLoop.h>
#include <string>
#include <unordered_map>

namespace drogon_pay
{

/**
 * @brief Per-thread cached HttpClient, one per target host.
 *
 * Replaces the old per-request drogon::HttpClient::newHttpClient() pattern
 * (which spawned a fresh client - and its own event loop thread - for every
 * outbound call, paying a TCP+TLS handshake each time). A thread_local map
 * gives every IO loop its own keep-alive client, equivalent to
 * drogon::IOThreadStorage but safe to call from non-IO threads too (the test
 * suite invokes channel methods from the main thread).
 */
inline drogon::HttpClientPtr cachedHttpClient(const std::string &hostString)
{
    thread_local std::unordered_map<std::string, drogon::HttpClientPtr> cache;
    auto &client = cache[hostString];
    if (!client)
    {
        auto *loop = trantor::EventLoop::getEventLoopOfCurrentThread();
        if (loop == nullptr)
        {
            loop = drogon::app().getLoop();
        }
        client = drogon::HttpClient::newHttpClient(hostString, loop);
    }
    return client;
}

}  // namespace drogon_pay
