#include "drogon_pay/ChannelRegistry.h"

#include <trantor/utils/Logger.h>

namespace drogon_pay
{

namespace
{
// Function-local static avoids the static-initialization-order fiasco and
// keeps the host-facing factory map alive for the whole process.
std::map<std::string, ChannelFactory> &factoryMap()
{
    static std::map<std::string, ChannelFactory> factories;
    return factories;
}
}  // namespace

void ChannelRegistry::registerFactory(const std::string &name, ChannelFactory factory)
{
    factoryMap()[name] = std::move(factory);
}

const std::map<std::string, ChannelFactory> &ChannelRegistry::factories()
{
    return factoryMap();
}

void ChannelRegistry::add(const std::string &name, PaymentChannelPtr channel)
{
    if (frozen_)
    {
        LOG_ERROR << "ChannelRegistry::add(" << name
                  << ") called after freeze(); registration ignored";
        return;
    }
    if (!channel)
    {
        LOG_ERROR << "ChannelRegistry::add(" << name << ") called with null channel";
        return;
    }
    channels_[name] = std::move(channel);
}

void ChannelRegistry::freeze()
{
    frozen_ = true;
}

PaymentChannelPtr ChannelRegistry::find(const std::string &name) const
{
    auto it = channels_.find(name);
    return it == channels_.end() ? nullptr : it->second;
}

std::vector<std::string> ChannelRegistry::names() const
{
    std::vector<std::string> result;
    result.reserve(channels_.size());
    for (const auto &[name, channel] : channels_)
    {
        (void)channel;
        result.push_back(name);
    }
    return result;
}

void ChannelRegistry::startAll() const
{
    for (const auto &[name, channel] : channels_)
    {
        LOG_INFO << "Starting payment channel: " << name;
        channel->onStart();
    }
}

void ChannelRegistry::stopAll() const
{
    for (const auto &[name, channel] : channels_)
    {
        LOG_INFO << "Stopping payment channel: " << name;
        channel->onStop();
    }
}

}  // namespace drogon_pay
