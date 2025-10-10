#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "audio/ClipSource.h"

namespace kraken::audio {

struct ClipStreamChunk {
    std::vector<std::vector<float>> channels;
    std::size_t frames{};
};

class ClipStream {
public:
    virtual ~ClipStream() = default;

    [[nodiscard]] virtual double sampleRate() const noexcept = 0;
    [[nodiscard]] virtual std::size_t channelCount() const noexcept = 0;
    [[nodiscard]] virtual const std::vector<ClipChannelMetadata>& metadata() const noexcept = 0;
    [[nodiscard]] virtual std::size_t chunkSize() const noexcept = 0;
    [[nodiscard]] virtual std::optional<std::size_t> totalFrames() const noexcept = 0;

    virtual bool next(ClipStreamChunk& chunk) = 0;
    virtual void reset() = 0;
};

struct ClipBuffer {
    std::shared_ptr<ClipStream> stream;
};

class Clip {
public:
    Clip(std::string name, ClipSource source, double lengthSeconds);

    [[nodiscard]] const std::string& name() const noexcept { return m_name; }
    [[nodiscard]] const ClipSource& source() const noexcept { return m_source; }
    [[nodiscard]] double lengthSeconds() const noexcept { return m_lengthSeconds; }

    [[nodiscard]] bool isPrepared() const noexcept { return m_buffer.has_value() && m_buffer->stream; }

    /// Loads the clip from the configured stem source into a temporary buffer.
    bool prepare(std::string* errorOut = nullptr);

    /// Releases the temporary buffer to maintain lightweight memory usage.
    void release();

    [[nodiscard]] const ClipBuffer* buffer() const noexcept { return m_buffer ? &*m_buffer : nullptr; }

private:
    std::string m_name;
    ClipSource m_source;
    double m_lengthSeconds;
    std::optional<ClipBuffer> m_buffer;
};

} // namespace kraken::audio
