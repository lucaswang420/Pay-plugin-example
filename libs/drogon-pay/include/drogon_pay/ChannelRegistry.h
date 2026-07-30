#pragma once

#include "PaymentChannel.h"

#include <json/json.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace drogon_pay
{

/**
 * @brief Registry of payment channels.
 *
 * Life cycle:
 *  1. Registration phase - single-threaded, during PayPlugin::initAndStart
 *     (built-in channels) and before drogon::app().run() (host-provided
 *     factories via registerFactory()).
 *  2. freeze() - the registry becomes an immutable read-only map.
 *  3. Runtime - find() is lock-free; concurrent lookups are safe because the
 *     underlying map is never mutated after freeze().
 *
 * Built-in channels are registered through the explicit
 * registerBuiltinChannels() call in PayPlugin (no self-registration macros:
 * static-library builds would drop those symbols).
 */
class ChannelRegistry
{
  public:
    /// Host extension point: register a factory for a custom channel BEFORE
    /// app().run(). The factory is invoked during PayPlugin::initAndStart with
    /// the channel's JSON config block when that channel is enabled.
    static void registerFactory(const std::string &name, ChannelFactory factory);

    /// Factories registered by hosts (consumed by PayPlugin during assembly).
    static const std::map<std::string, ChannelFactory> &factories();

    ChannelRegistry() = default;

    ChannelRegistry(const ChannelRegistry &) = delete;
    ChannelRegistry &operator=(const ChannelRegistry &) = delete;

    /// Registration phase only (asserts when called after freeze()).
    void add(const std::string &name, PaymentChannelPtr channel);

    /// Ends the registration phase; the registry is read-only afterwards.
    void freeze();

    bool frozen() const
    {
        return frozen_;
    }

    /// Runtime lookup. Returns nullptr for unknown channels - callers must
    /// map that to CHANNEL_NOT_AVAILABLE (no fallback channel).
    PaymentChannelPtr find(const std::string &name) const;

    /// Names of all registered channels (for startup logging/metrics).
    std::vector<std::string> names() const;

    std::size_t size() const
    {
        return channels_.size();
    }

    /// Invoke onStart()/onStop() on every registered channel.
    void startAll() const;
    void stopAll() const;

  private:
    std::map<std::string, PaymentChannelPtr> channels_;
    bool frozen_{false};
};

}  // namespace drogon_pay
