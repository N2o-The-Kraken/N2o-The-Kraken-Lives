#include "audio/Clip.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace kraken::audio {

Clip::Clip(std::string name, std::filesystem::path zipPath, double lengthSeconds)
    : m_name(std::move(name)),
      m_zipPath(std::move(zipPath)),
      m_lengthSeconds(lengthSeconds) {}

void Clip::prepare() {
    if (m_buffer) {
        return;
    }

    ClipBuffer buffer;
    if (!std::filesystem::exists(m_zipPath)) {
        std::cerr << "Warning: clip archive missing at " << m_zipPath << '\n';
        m_buffer = std::move(buffer);
        return;
    }

    std::ifstream stream(m_zipPath, std::ios::binary);
    if (!stream) {
        std::cerr << "Warning: failed to open zip archive " << m_zipPath << '\n';
        m_buffer = std::move(buffer);
        return;
    }

    stream.seekg(0, std::ios::end);
    const auto size = static_cast<std::size_t>(stream.tellg());
    stream.seekg(0, std::ios::beg);
    buffer.data.resize(size);
    stream.read(reinterpret_cast<char*>(buffer.data.data()), size);

    buffer.channels = 4; // stems are delivered as 4-way splits
    buffer.frames = static_cast<std::size_t>(m_lengthSeconds * 48000);

    m_buffer = std::move(buffer);
}

void Clip::release() {
    m_buffer.reset();
}

} // namespace kraken::audio
