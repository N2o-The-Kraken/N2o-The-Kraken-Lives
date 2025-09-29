#include "audio/Clip.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace kraken::audio {

namespace {
std::vector<std::filesystem::path> wavFilesInDirectory(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> files;
    if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
        return files;
    }

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        auto extension = entry.path().extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        if (extension == ".wav" || extension == ".wave") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

void appendFileToBuffer(const std::filesystem::path& file, ClipBuffer& buffer) {
    std::ifstream stream(file, std::ios::binary);
    if (!stream) {
        std::cerr << "Warning: failed to open stem file " << file << '\n';
        return;
    }

    stream.seekg(0, std::ios::end);
    const auto size = static_cast<std::size_t>(stream.tellg());
    stream.seekg(0, std::ios::beg);

    const auto startIndex = buffer.data.size();
    buffer.data.resize(startIndex + size);
    stream.read(reinterpret_cast<char*>(buffer.data.data() + startIndex), size);
}

} // namespace

Clip::Clip(std::string name, ClipSource source, double lengthSeconds)
    : m_name(std::move(name)),
      m_source(std::move(source)),
      m_lengthSeconds(lengthSeconds) {}

void Clip::prepare() {
    if (m_buffer) {
        return;
    }

    ClipBuffer buffer;
    const auto& stemPath = m_source.path;

    switch (m_source.type) {
        case ClipSourceType::ZipArchive: {
            if (!std::filesystem::exists(stemPath)) {
                std::cerr << "Warning: clip archive missing at " << stemPath << '\n';
                break;
            }

            std::ifstream stream(stemPath, std::ios::binary);
            if (!stream) {
                std::cerr << "Warning: failed to open zip archive " << stemPath << '\n';
                break;
            }

            stream.seekg(0, std::ios::end);
            const auto size = static_cast<std::size_t>(stream.tellg());
            stream.seekg(0, std::ios::beg);
            buffer.data.resize(size);
            stream.read(reinterpret_cast<char*>(buffer.data.data()), size);
            buffer.channels = 4; // stems are delivered as 4-way splits
            break;
        }
        case ClipSourceType::WavDirectory: {
            const auto files = wavFilesInDirectory(stemPath);
            if (files.empty()) {
                std::cerr << "Warning: no WAV files found in directory " << stemPath << '\n';
                break;
            }
            for (const auto& file : files) {
                appendFileToBuffer(file, buffer);
            }
            buffer.channels = files.size();
            break;
        }
        case ClipSourceType::AudioFile: {
            if (!std::filesystem::exists(stemPath)) {
                std::cerr << "Warning: audio file missing at " << stemPath << '\n';
                break;
            }
            appendFileToBuffer(stemPath, buffer);
            buffer.channels = 2; // assume stereo render for single file stems
            break;
        }
    }

    buffer.frames = static_cast<std::size_t>(m_lengthSeconds * 48000);
    m_buffer = std::move(buffer);
}

void Clip::release() {
    m_buffer.reset();
}

} // namespace kraken::audio
