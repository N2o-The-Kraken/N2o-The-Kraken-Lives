#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace kraken::audio {

struct ClipBuffer {
    std::vector<std::byte> data;
    std::size_t channels{};
    std::size_t frames{};
};

class Clip {
public:
    Clip(std::string name, std::filesystem::path zipPath, double lengthSeconds);

    [[nodiscard]] const std::string& name() const noexcept { return m_name; }
    [[nodiscard]] const std::filesystem::path& zipPath() const noexcept { return m_zipPath; }
    [[nodiscard]] double lengthSeconds() const noexcept { return m_lengthSeconds; }

    [[nodiscard]] bool isPrepared() const noexcept { return m_buffer.has_value(); }

    /// Loads the clip from the zipped stem archive into a temporary buffer.
    void prepare();

    /// Releases the temporary buffer to maintain lightweight memory usage.
    void release();

    [[nodiscard]] const ClipBuffer* buffer() const noexcept { return m_buffer ? &*m_buffer : nullptr; }

private:
    std::string m_name;
    std::filesystem::path m_zipPath;
    double m_lengthSeconds;
    std::optional<ClipBuffer> m_buffer;
};

} // namespace kraken::audio
