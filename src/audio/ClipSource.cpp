#include "audio/ClipSource.h"

namespace kraken::audio {

std::string clipSourceTypeName(ClipSourceType type) {
    switch (type) {
        case ClipSourceType::ZipArchive:
            return "zip";
        case ClipSourceType::WavDirectory:
            return "wav_dir";
        case ClipSourceType::AudioFile:
            return "audio_file";
    }
    return "unknown";
}

} // namespace kraken::audio
