#include "audio/StemResolver.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

namespace kraken::audio {

namespace {

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string unescapeJsonString(const std::string& input) {
    std::string output;
    output.reserve(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        const char ch = input[i];
        if (ch == '\\' && i + 1 < input.size()) {
            const char next = input[++i];
            switch (next) {
                case '\\':
                    output.push_back('\\');
                    break;
                case '"':
                    output.push_back('"');
                    break;
                case 'n':
                    output.push_back('\n');
                    break;
                case 'r':
                    output.push_back('\r');
                    break;
                case 't':
                    output.push_back('\t');
                    break;
                default:
                    output.push_back(next);
                    break;
            }
        } else {
            output.push_back(ch);
        }
    }
    return output;
}

std::optional<ClipSourceType> parseClipSourceType(const std::string& rawType) {
    const auto type = toLower(rawType);
    if (type == "zip") {
        return ClipSourceType::ZipArchive;
    }
    if (type == "wav_dir" || type == "wav-dir" || type == "wavdir") {
        return ClipSourceType::WavDirectory;
    }
    if (type == "audio_file" || type == "audio-file" || type == "audiofile") {
        return ClipSourceType::AudioFile;
    }
    return std::nullopt;
}

std::unordered_map<std::string, ClipSource> parseManifest(
    const std::filesystem::path& manifestPath,
    std::vector<std::string>& warnings) {
    std::unordered_map<std::string, ClipSource> entries;

    std::ifstream stream(manifestPath);
    if (!stream) {
        warnings.emplace_back("Unable to open stem manifest at " + manifestPath.string());
        return entries;
    }

    std::string contents((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

    const std::regex entryRegex(R"REGEX("([^"]+)"\s*:\s*\{([^}]*)\})REGEX");
    const std::regex typeRegex(R"REGEX("type"\s*:\s*"([^"]+)")REGEX");
    const std::regex pathRegex(R"REGEX("path"\s*:\s*"([^"]*)")REGEX");

    const auto begin = std::sregex_iterator(contents.begin(), contents.end(), entryRegex);
    const auto end = std::sregex_iterator();

    for (auto it = begin; it != end; ++it) {
        const auto& match = *it;
        const std::string trackId = unescapeJsonString(match[1].str());
        const std::string body = match[2].str();

        std::smatch typeMatch;
        if (!std::regex_search(body, typeMatch, typeRegex)) {
            warnings.emplace_back("Stem manifest entry for " + trackId + " is missing a type");
            continue;
        }
        std::smatch pathMatch;
        if (!std::regex_search(body, pathMatch, pathRegex)) {
            warnings.emplace_back("Stem manifest entry for " + trackId + " is missing a path");
            continue;
        }

        const auto maybeType = parseClipSourceType(unescapeJsonString(typeMatch[1].str()));
        if (!maybeType.has_value()) {
            warnings.emplace_back("Stem manifest entry for " + trackId + " has an unknown type");
            continue;
        }

        std::filesystem::path path = unescapeJsonString(pathMatch[1].str());
        if (path.empty()) {
            warnings.emplace_back("Stem manifest entry for " + trackId + " has an empty path");
            continue;
        }

        if (path.is_relative()) {
            path = (manifestPath.parent_path() / path).lexically_normal();
        }

        entries.emplace(trackId, ClipSource{*maybeType, std::move(path)});
    }

    return entries;
}

bool pathLooksLikeZip(const std::filesystem::path& path) {
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return extension == ".zip";
}

bool validateSourcePath(const std::string& trackId, const ClipSource& source) {
    switch (source.type) {
        case ClipSourceType::ZipArchive: {
            if (!pathLooksLikeZip(source.path)) {
                std::cerr << "Warning: manifest entry for " << trackId
                          << " is marked as zip but does not use a .zip extension (" << source.path << ")\n";
                return false;
            }
            if (!std::filesystem::exists(source.path) || !std::filesystem::is_regular_file(source.path)) {
                std::cerr << "Warning: manifest entry for " << trackId
                          << " points to missing archive " << source.path << '\n';
                return false;
            }
            return true;
        }
        case ClipSourceType::WavDirectory: {
            if (!std::filesystem::exists(source.path) || !std::filesystem::is_directory(source.path)) {
                std::cerr << "Warning: manifest entry for " << trackId
                          << " points to missing directory " << source.path << '\n';
                return false;
            }
            return true;
        }
        case ClipSourceType::AudioFile: {
            if (!std::filesystem::exists(source.path) || !std::filesystem::is_regular_file(source.path)) {
                std::cerr << "Warning: manifest entry for " << trackId
                          << " points to missing audio file " << source.path << '\n';
                return false;
            }
            return true;
        }
    }
    return false;
}

} // namespace

std::filesystem::path defaultStemManifestPath() {
    return std::filesystem::path{"assets"} / "stems" / "stems_manifest.json";
}

std::filesystem::path locateStemManifest() {
    if (const char* overridePath = std::getenv("KRAKEN_STEM_MANIFEST"); overridePath && *overridePath) {
        return std::filesystem::path(overridePath);
    }
    return defaultStemManifestPath();
}

std::optional<ClipSource> manifestClipSource(const std::string& trackId) {
    const auto manifestPath = locateStemManifest();
    std::vector<std::string> warnings;

    if (!std::filesystem::exists(manifestPath)) {
        return std::nullopt;
    }

    const auto entries = parseManifest(manifestPath, warnings);

    for (const auto& warning : warnings) {
        std::cerr << "Warning: " << warning << '\n';
    }

    const auto it = entries.find(trackId);
    if (it == entries.end()) {
        return std::nullopt;
    }

    if (!validateSourcePath(trackId, it->second)) {
        return std::nullopt;
    }

    return it->second;
}

ClipSource resolveClipSource(const std::string& trackId) {
    if (auto fromManifest = manifestClipSource(trackId)) {
        return *fromManifest;
    }

    ClipSource fallback;
    fallback.type = ClipSourceType::ZipArchive;
    fallback.path = std::filesystem::path{"stems"} / (trackId + ".zip");
    return fallback;
}

} // namespace kraken::audio
