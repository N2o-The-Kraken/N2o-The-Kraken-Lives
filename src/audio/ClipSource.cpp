#include "audio/ClipSource.h"

#include <type_traits>
#include <variant>
#include <vector>

namespace kraken::audio {

ClipSourceType clipSourceType(const ClipSource& source) {
    return std::visit(
        [](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, ZipClipSource>) {
                return ClipSourceType::ZipArchive;
            } else if constexpr (std::is_same_v<T, WavDirectoryClipSource>) {
                return ClipSourceType::WavDirectory;
            } else {
                return ClipSourceType::AudioFile;
            }
        },
        source.source);
}

const std::vector<ClipChannelMetadata>& clipChannelMetadata(const ClipSource& source) {
    return std::visit(
        [](const auto& value) -> const std::vector<ClipChannelMetadata>& {
            return value.channels;
        },
        source.source);
}

std::filesystem::path clipSourcePath(const ClipSource& source) {
    return std::visit(
        [](const auto& value) -> std::filesystem::path {
            if constexpr (std::is_same_v<std::decay_t<decltype(value)>, ZipClipSource>) {
                return value.archive;
            } else if constexpr (std::is_same_v<std::decay_t<decltype(value)>, WavDirectoryClipSource>) {
                return value.directory;
            } else {
                return value.file;
            }
        },
        source.source);
}

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
