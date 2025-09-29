#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "audio/ClipSource.h"

namespace kraken::audio {

std::filesystem::path defaultStemManifestPath();
std::filesystem::path locateStemManifest();
std::optional<ClipSource> manifestClipSource(const std::string& trackId);
ClipSource resolveClipSource(const std::string& trackId);

} // namespace kraken::audio
