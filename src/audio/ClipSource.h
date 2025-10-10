#pragma once

#include <filesystem>
#include <string>
#include <variant>
#include <vector>

namespace kraken::audio {

enum class ClipSourceType {
    ZipArchive,
    WavDirectory,
    AudioFile,
};

struct ClipChannelMetadata {
    std::string id;
    std::string description;
};

struct ZipClipSource {
    std::filesystem::path archive;
    std::vector<ClipChannelMetadata> channels;
};

struct WavDirectoryClipSource {
    std::filesystem::path directory;
    std::vector<ClipChannelMetadata> channels;
};

struct AudioFileClipSource {
    std::filesystem::path file;
    bool isMp3{false};
    std::vector<ClipChannelMetadata> channels;
};

using ClipSourceVariant = std::variant<ZipClipSource, WavDirectoryClipSource, AudioFileClipSource>;

struct ClipSource {
    ClipSourceVariant source;
};

ClipSourceType clipSourceType(const ClipSource& source);
const std::vector<ClipChannelMetadata>& clipChannelMetadata(const ClipSource& source);
std::filesystem::path clipSourcePath(const ClipSource& source);
std::string clipSourceTypeName(ClipSourceType type);

} // namespace kraken::audio
