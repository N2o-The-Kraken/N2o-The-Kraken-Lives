#include "audio/Mixer.h"

namespace kraken::audio {

Mixer::ChannelId Mixer::createChannel(const std::string& name) {
    m_names.push_back(name);
    m_channels.emplace_back();
    return m_channels.size() - 1;
}

void Mixer::removeChannel(ChannelId id) {
    if (id >= m_channels.size()) {
        return;
    }
    m_channels.erase(m_channels.begin() + static_cast<std::ptrdiff_t>(id));
    m_names.erase(m_names.begin() + static_cast<std::ptrdiff_t>(id));
}

const ChannelSettings* Mixer::channel(ChannelId id) const {
    if (id >= m_channels.size()) {
        return nullptr;
    }
    return &m_channels[id];
}

void Mixer::updateChannel(ChannelId id, const ChannelSettings& settings) {
    if (id >= m_channels.size()) {
        return;
    }
    m_channels[id] = settings;
}

std::vector<std::string> Mixer::channelNames() const {
    return m_names;
}

void Mixer::forEachChannel(const std::function<void(ChannelId, const ChannelSettings&)>& fn) const {
    for (std::size_t i = 0; i < m_channels.size(); ++i) {
        fn(i, m_channels[i]);
    }
}

} // namespace kraken::audio
