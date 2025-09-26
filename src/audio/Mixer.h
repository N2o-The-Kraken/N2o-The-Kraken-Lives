#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace kraken::audio {

struct ChannelSettings {
    double volume{0.8};
    double pan{0.0};
    std::unordered_map<std::string, double> sendLevels;
};

class Mixer {
public:
    using ChannelId = std::size_t;

    Mixer() = default;

    ChannelId createChannel(const std::string& name);
    void removeChannel(ChannelId id);

    [[nodiscard]] const ChannelSettings* channel(ChannelId id) const;
    void updateChannel(ChannelId id, const ChannelSettings& settings);

    [[nodiscard]] std::vector<std::string> channelNames() const;

    void forEachChannel(const std::function<void(ChannelId, const ChannelSettings&)>& fn) const;

private:
    std::vector<std::string> m_names;
    std::vector<ChannelSettings> m_channels;
};

} // namespace kraken::audio
