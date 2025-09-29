#pragma once

#include <filesystem>
#include <string>

namespace kraken::audio {

enum class ClipSourceType {
    ZipArchive,
    WavDirectory,
    AudioFile,
};

struct ClipSource {
    ClipSourceType type{ClipSourceType::ZipArchive};
    std::filesystem::path path;
};

std::string clipSourceTypeName(ClipSourceType type);

} // namespace kraken::audio
