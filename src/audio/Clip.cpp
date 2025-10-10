#include "audio/Clip.h"

#include "audio/ClipLoader.h"

#include <iostream>
#include <string>

namespace kraken::audio {

Clip::Clip(std::string name, ClipSource source, double lengthSeconds)
    : m_name(std::move(name)),
      m_source(std::move(source)),
      m_lengthSeconds(lengthSeconds) {}

bool Clip::prepare(std::string* errorOut) {
    if (m_buffer && m_buffer->stream) {
        return true;
    }

    ClipLoader loader;
    const auto result = loader.load(m_source, m_lengthSeconds);
    for (const auto& warning : result.warnings) {
        std::cerr << "Warning: " << warning << '\n';
    }

    if (!result) {
        const std::string message = "Error: failed to load clip " + m_name + ": " + result.error;
        std::cerr << message << '\n';
        if (errorOut) {
            *errorOut = result.error;
        }
        return false;
    }

    m_buffer = std::move(*result.buffer);
    return true;
}

void Clip::release() {
    m_buffer.reset();
}

} // namespace kraken::audio
