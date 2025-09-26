#pragma once

#include <cstddef>
#include <string>

namespace kraken::platform {

enum class PlatformType {
    Windows,
    Android
};

struct PlatformConfig {
    PlatformType type{PlatformType::Windows};
    double sampleRate{48000.0};
    std::size_t blockSize{256};
    bool enableMidi{true};
    bool enableTouch{true};
    bool enableVibration{false};
};

class Platform {
public:
    explicit Platform(PlatformConfig config);

    [[nodiscard]] const PlatformConfig& config() const noexcept { return m_config; }

    [[nodiscard]] std::string describe() const;

private:
    PlatformConfig m_config;
};

} // namespace kraken::platform
