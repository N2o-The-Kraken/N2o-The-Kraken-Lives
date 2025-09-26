#include "platform/Platform.h"

#include <sstream>

namespace kraken::platform {

Platform::Platform(PlatformConfig config) : m_config(config) {}

std::string Platform::describe() const {
    std::ostringstream oss;
    oss << (m_config.type == PlatformType::Windows ? "Windows 10/11" : "Android 7-15");
    oss << " | sample rate: " << m_config.sampleRate;
    oss << " | buffer: " << m_config.blockSize << " frames";
    oss << " | MIDI: " << (m_config.enableMidi ? "on" : "off");
    if (m_config.type == PlatformType::Android) {
        oss << " | touch: " << (m_config.enableTouch ? "enabled" : "disabled");
        oss << " | vibration: " << (m_config.enableVibration ? "enabled" : "disabled");
    }
    return oss.str();
}

} // namespace kraken::platform
