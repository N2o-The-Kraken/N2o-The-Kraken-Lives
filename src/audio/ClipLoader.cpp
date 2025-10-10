#include "audio/ClipLoader.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <initializer_list>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "audio/ClipSource.h"

#ifndef KRAKEN_HAS_SNDFILE
#define KRAKEN_HAS_SNDFILE 0
#endif
#ifndef KRAKEN_HAS_MINIZIP
#define KRAKEN_HAS_MINIZIP 0
#endif
#ifndef KRAKEN_HAS_MINIMP3
#define KRAKEN_HAS_MINIMP3 0
#endif

#if KRAKEN_HAS_SNDFILE
#include <sndfile.h>
#endif
#if KRAKEN_HAS_MINIZIP
#include <minizip/unzip.h>
#endif
#if KRAKEN_HAS_MINIMP3
#include <minimp3/minimp3_ex.h>
#endif

namespace kraken::audio {
namespace {

constexpr std::size_t kDefaultChunkFrames = 4096;

ClipLoader::Result makeError(std::string message) {
    ClipLoader::Result result;
    result.error = std::move(message);
    return result;
}

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_FLOAT_OUTPUT
#include <minimp3/minimp3_ex.h>
#include <minizip/unzip.h>
#include <sndfile.h>

namespace kraken::audio {

namespace {
constexpr std::size_t kDefaultChunkFrames = 4096;

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool hasExtension(const std::filesystem::path& path, std::initializer_list<std::string_view> exts) {
    const auto ext = lowercase(path.extension().string());
    for (std::string_view candidate : exts) {
        if (ext == lowercase(std::string(candidate))) {
            return true;
        }
    }
    return false;
}

ClipChannelMetadata defaultChannelMetadata(std::size_t index) {
    static const std::array<const char*, 4> defaults = {"Drums", "Bass", "Music", "Vocals"};
    ClipChannelMetadata metadata;
    metadata.id = "channel_" + std::to_string(index + 1);
    if (index < defaults.size()) {
        metadata.description = defaults[index];
bool hasExtension(const std::filesystem::path& path, std::string extension) {
    auto ext = lowercase(path.extension().string());
    extension = lowercase(std::move(extension));
    return ext == extension;
}

ClipChannelMetadata defaultChannelMetadata(std::size_t index) {
    static const std::array<std::string, 4> defaultDescriptions = {"Drums", "Bass", "Music", "Vocals"};
    ClipChannelMetadata metadata;
    metadata.id = "channel_" + std::to_string(index + 1);
    if (index < defaultDescriptions.size()) {
        metadata.description = defaultDescriptions[index];
    } else {
        metadata.description = "Stem " + std::to_string(index + 1);
    }
    return metadata;
}

std::vector<ClipChannelMetadata> ensureMetadata(const std::vector<ClipChannelMetadata>& provided,
                                                std::size_t required) {
    std::vector<ClipChannelMetadata> result;
    result.reserve(required);
    for (std::size_t i = 0; i < required; ++i) {
        if (i < provided.size()) {
            result.push_back(provided[i]);
        } else {
            result.push_back(defaultChannelMetadata(i));
                                                std::size_t required,
                                                std::size_t startIndex = 0) {
    std::vector<ClipChannelMetadata> result;
    result.reserve(required);
    for (std::size_t i = 0; i < required; ++i) {
        const std::size_t index = startIndex + i;
        if (index < provided.size()) {
            result.push_back(provided[index]);
        } else {
            result.push_back(defaultChannelMetadata(index));
        }
    }
    return result;
}

#if KRAKEN_HAS_SNDFILE

struct MemoryBuffer {
    std::vector<std::uint8_t> bytes;
    sf_count_t position{};
};

sf_count_t memoryGetFilelen(void* userData) {
    auto* memory = static_cast<MemoryBuffer*>(userData);
    return static_cast<sf_count_t>(memory->bytes.size());
}

sf_count_t memorySeek(sf_count_t offset, int whence, void* userData) {
    auto* memory = static_cast<MemoryBuffer*>(userData);
    sf_count_t newPosition = memory->position;
    switch (whence) {
        case SEEK_SET:
            newPosition = offset;
            break;
        case SEEK_CUR:
            newPosition += offset;
            break;
        case SEEK_END:
            newPosition = static_cast<sf_count_t>(memory->bytes.size()) + offset;
            break;
        default:
            return -1;
    }

    if (newPosition < 0 || newPosition > static_cast<sf_count_t>(memory->bytes.size())) {
        return -1;
    }

    memory->position = newPosition;
    return memory->position;
}

sf_count_t memoryRead(void* ptr, sf_count_t count, void* userData) {
    auto* memory = static_cast<MemoryBuffer*>(userData);
    if (count <= 0) {
        return 0;
    }

    const auto remaining = static_cast<sf_count_t>(memory->bytes.size()) - memory->position;
    const auto toCopy = std::min(count, remaining);
    if (toCopy > 0) {
        std::memcpy(ptr, memory->bytes.data() + memory->position, static_cast<std::size_t>(toCopy));
        memory->position += toCopy;
    }
    return toCopy;
}

sf_count_t memoryWrite(const void*, sf_count_t, void*) {
    return 0;
}

sf_count_t memoryTell(void* userData) {
    auto* memory = static_cast<MemoryBuffer*>(userData);
    return memory->position;
}

const SF_VIRTUAL_IO kMemoryIO = {
    memoryGetFilelen,
    memorySeek,
    memoryRead,
    memoryWrite,
    memoryTell,
};

struct SndfileHandle {
    std::filesystem::path path;
    std::unique_ptr<SNDFILE, decltype(&sf_close)> file;
    SF_INFO info{};
    std::shared_ptr<void> backing;

    SndfileHandle(std::filesystem::path p, SNDFILE* f, const SF_INFO& i)
        : path(std::move(p)), file(f, &sf_close), info(i) {}

    SndfileHandle(std::filesystem::path p, SNDFILE* f, const SF_INFO& i, std::shared_ptr<void> backingStorage)
        : path(std::move(p)), file(f, &sf_close), info(i), backing(std::move(backingStorage)) {}

    SndfileHandle(SndfileHandle&&) noexcept = default;
    SndfileHandle& operator=(SndfileHandle&&) noexcept = default;

    SndfileHandle(const SndfileHandle&) = delete;
    SndfileHandle& operator=(const SndfileHandle&) = delete;
};

class SndfileClipStream : public ClipStream {
public:
    SndfileClipStream(std::vector<SndfileHandle> handles,
                      std::vector<ClipChannelMetadata> metadata,
                      std::size_t chunkFrames)
    SNDFILE* file{};
    SF_INFO info{};
    std::function<bool()> reset;
    std::function<void()> close;
    std::shared_ptr<MemoryBuffer> memory;
};

class AggregateSndfileStream : public ClipStream {
public:
    AggregateSndfileStream(std::vector<SndfileHandle> handles,
                           std::vector<ClipChannelMetadata> metadata,
                           std::size_t chunkFrames)
        : m_handles(std::move(handles)),
          m_metadata(std::move(metadata)),
          m_chunkFrames(chunkFrames) {
        if (m_handles.empty()) {
            m_error = "No audio sources available";
            return;
        }

        m_finished.assign(m_handles.size(), false);
        m_totalChannels = 0;
        sf_count_t maxFrames = 0;
        m_sampleRate = static_cast<double>(m_handles.front().info.samplerate);
        m_framesRead.assign(m_handles.size(), 0);
        m_totalChannels = 0;
        sf_count_t maxFrames = 0;
        for (const auto& handle : m_handles) {
            m_totalChannels += static_cast<std::size_t>(handle.info.channels);
            if (handle.info.frames > maxFrames) {
                maxFrames = handle.info.frames;
            }
        }

        if (m_metadata.size() < m_totalChannels) {
            m_metadata = ensureMetadata(m_metadata, m_totalChannels);
        }

            auto extended = ensureMetadata(m_metadata, m_totalChannels);
            m_metadata = std::move(extended);
        }

        m_sampleRate = static_cast<double>(m_handles.front().info.samplerate);
        if (maxFrames > 0) {
            m_totalFrames = static_cast<std::size_t>(maxFrames);
        }

        m_valid = true;
    }

    ~AggregateSndfileStream() override {
        for (auto& handle : m_handles) {
            if (handle.file && handle.close) {
                handle.close();
                handle.file = nullptr;
            }
        }
    }

    [[nodiscard]] double sampleRate() const noexcept override { return m_sampleRate; }
    [[nodiscard]] std::size_t channelCount() const noexcept override { return m_totalChannels; }
    [[nodiscard]] const std::vector<ClipChannelMetadata>& metadata() const noexcept override { return m_metadata; }
    [[nodiscard]] std::size_t chunkSize() const noexcept override { return m_chunkFrames; }
    [[nodiscard]] std::optional<std::size_t> totalFrames() const noexcept override { return m_totalFrames; }

    bool next(ClipStreamChunk& chunk) override {
        if (!m_valid) {
            return false;
        }

        chunk.channels.assign(m_totalChannels, {});
        chunk.frames = 0;

        std::size_t channelCursor = 0;
        std::size_t maxFrames = 0;

        for (std::size_t i = 0; i < m_handles.size(); ++i) {
            auto& handle = m_handles[i];
            const auto channels = static_cast<std::size_t>(handle.info.channels);

            if (m_finished[i]) {
                for (std::size_t ch = 0; ch < channels; ++ch) {
                    chunk.channels[channelCursor++].clear();
                }
                continue;
            }

            m_readBuffer.resize(channels * m_chunkFrames);
            const sf_count_t framesRead = sf_readf_float(handle.file.get(),
                                                         m_readBuffer.data(),
                                                         static_cast<sf_count_t>(m_chunkFrames));
            if (framesRead <= 0) {
                m_finished[i] = true;
                for (std::size_t ch = 0; ch < channels; ++ch) {
                    chunk.channels[channelCursor++].clear();
                }
                continue;
            }

            const std::size_t frames = static_cast<std::size_t>(framesRead);
            maxFrames = std::max(maxFrames, frames);

            for (std::size_t ch = 0; ch < channels; ++ch) {
                auto& dest = chunk.channels[channelCursor++];
                dest.resize(frames);
                for (std::size_t frame = 0; frame < frames; ++frame) {
                    dest[frame] = m_readBuffer[frame * channels + ch];
                }
            }

            if (frames < m_chunkFrames) {
        if (std::all_of(m_finished.begin(), m_finished.end(), [](bool finished) { return finished; })) {
            return false;
        }

        const std::size_t handleCount = m_handles.size();
        std::vector<std::size_t> framesPerHandle(handleCount, 0);
        std::vector<std::vector<std::vector<float>>> perHandle(handleCount);

        bool anyFrames = false;
        std::size_t maxFrames = 0;

        for (std::size_t i = 0; i < handleCount; ++i) {
            if (m_finished[i]) {
                continue;
            }

            auto& handle = m_handles[i];
            const auto channels = static_cast<std::size_t>(handle.info.channels);
            perHandle[i].resize(channels);
            const std::size_t requiredSamples = channels * m_chunkFrames;
            if (m_interleavedScratch.size() < requiredSamples) {
                m_interleavedScratch.resize(requiredSamples);
            }

            const sf_count_t read = sf_readf_float(handle.file,
                                                   m_interleavedScratch.data(),
                                                   static_cast<sf_count_t>(m_chunkFrames));
            if (read < 0) {
                m_error = "Error decoding audio stream";
                m_valid = false;
                return false;
            }

            const std::size_t frames = static_cast<std::size_t>(read);
            framesPerHandle[i] = frames;
            if (frames == 0) {
                m_finished[i] = true;
                continue;
            }

            anyFrames = true;
            maxFrames = std::max(maxFrames, frames);

            for (std::size_t ch = 0; ch < channels; ++ch) {
                auto& dest = perHandle[i][ch];
                dest.resize(frames);
                for (std::size_t frame = 0; frame < frames; ++frame) {
                    dest[frame] = m_interleavedScratch[frame * channels + ch];
                }
            }

            m_framesRead[i] += static_cast<sf_count_t>(frames);
            if (handle.info.frames > 0 && m_framesRead[i] >= handle.info.frames) {
                m_finished[i] = true;
            }
        }

        if (maxFrames == 0) {
            return false;
        }

        for (auto& channel : chunk.channels) {
            if (channel.size() < maxFrames) {
                channel.resize(maxFrames, 0.0f);
            }
        }

        chunk.frames = maxFrames;
        if (!anyFrames) {
            return false;
        }

        chunk.frames = maxFrames;
        chunk.channels.resize(m_totalChannels);

        std::size_t channelOffset = 0;
        for (std::size_t i = 0; i < handleCount; ++i) {
            const auto channels = static_cast<std::size_t>(m_handles[i].info.channels);
            for (std::size_t ch = 0; ch < channels; ++ch) {
                auto& dest = chunk.channels[channelOffset + ch];
                dest.assign(maxFrames, 0.0f);
                if (ch < perHandle[i].size() && !perHandle[i][ch].empty()) {
                    const auto& src = perHandle[i][ch];
                    std::copy(src.begin(), src.end(), dest.begin());
                }
            }
            channelOffset += channels;
        }

        return true;
    }

    void reset() override {
        for (std::size_t i = 0; i < m_handles.size(); ++i) {
            auto& handle = m_handles[i];
            if (handle.file) {
                sf_seek(handle.file.get(), 0, SEEK_SET);
            }
            if (i < m_finished.size()) {
                m_finished[i] = false;
            }
        }
    }

private:
    std::vector<SndfileHandle> m_handles;
    std::vector<ClipChannelMetadata> m_metadata;
    std::vector<bool> m_finished;
    std::vector<float> m_readBuffer;
    std::size_t m_chunkFrames{};
    std::size_t m_totalChannels{};
    double m_sampleRate{};
    std::optional<std::size_t> m_totalFrames;
    bool m_valid{false};
};

ClipLoader::Result makeStream(std::vector<SndfileHandle> handles,
                              std::vector<ClipChannelMetadata> metadata,
                              double expectedLengthSeconds,
                              std::vector<std::string> warnings) {
    if (handles.empty()) {
        return makeError("No audio sources were decoded.");
    }

    std::size_t totalChannels = 0;
    for (const auto& handle : handles) {
        totalChannels += static_cast<std::size_t>(handle.info.channels);
    }

    if (metadata.size() < totalChannels) {
        metadata = ensureMetadata(metadata, totalChannels);
    }

    auto stream = std::make_shared<SndfileClipStream>(std::move(handles), std::move(metadata), kDefaultChunkFrames);
    if (!stream || stream->channelCount() == 0) {
        return makeError("Failed to initialise the audio stream.");
    }

    ClipBuffer buffer;
    buffer.stream = std::move(stream);

    ClipLoader::Result result;
    result.buffer = std::move(buffer);
    result.warnings = std::move(warnings);

    if (result.buffer && result.buffer->stream) {
        const auto frames = result.buffer->stream->totalFrames();
        if (frames && result.buffer->stream->sampleRate() > 0.0) {
            const double durationSeconds = static_cast<double>(*frames) / result.buffer->stream->sampleRate();
            if (expectedLengthSeconds > 0.0 && std::abs(durationSeconds - expectedLengthSeconds) > 0.5) {
                std::ostringstream oss;
                oss << "Expected clip length of " << expectedLengthSeconds
                    << "s but decoded " << durationSeconds << "s of audio.";
                result.warnings.push_back(oss.str());
            }
        }
    }

    return result;
}

#endif // KRAKEN_HAS_SNDFILE

ClipLoader::Result loadWavDirectory(const WavDirectoryClipSource& source, double expectedLengthSeconds) {
#if KRAKEN_HAS_SNDFILE
    namespace fs = std::filesystem;

    if (!fs::exists(source.directory)) {
        return makeError("WAV directory does not exist: " + source.directory.string());
    }
    if (!fs::is_directory(source.directory)) {
        return makeError("Expected a directory of WAV stems at: " + source.directory.string());
    }

    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(source.directory)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (hasExtension(entry.path(), {".wav", ".wave", ".aiff", ".aif"})) {
            files.push_back(entry.path());
        }
    }

    std::sort(files.begin(), files.end());
    if (files.empty()) {
        return makeError("No WAV files were found in directory: " + source.directory.string());
    }

    std::vector<SndfileHandle> handles;
    handles.reserve(files.size());
    double referenceRate = 0.0;
    bool sampleMismatch = false;

    for (const auto& file : files) {
        SF_INFO info{};
        SNDFILE* snd = sf_open(file.string().c_str(), SFM_READ, &info);
        if (!snd) {
            return makeError("Failed to open audio file '" + file.string() + "': " + std::string(sf_strerror(nullptr)));
        }

        if (referenceRate == 0.0) {
            referenceRate = static_cast<double>(info.samplerate);
        } else if (static_cast<double>(info.samplerate) != referenceRate) {
            sampleMismatch = true;
        }

        handles.emplace_back(file, snd, info);
    }

    std::vector<std::string> warnings;
    if (sampleMismatch) {
        warnings.emplace_back("Input stems use different sample rates; playback will follow the first file's rate.");
    }

    auto metadata = source.channels;
    return makeStream(std::move(handles), std::move(metadata), expectedLengthSeconds, std::move(warnings));
#else
    (void)source;
    (void)expectedLengthSeconds;
    return makeError("WAV stem decoding is unavailable because libsndfile support is disabled at build time.");
#endif
}

ClipLoader::Result loadZipArchive(const ZipClipSource& source, double expectedLengthSeconds) {
#if !KRAKEN_HAS_SNDFILE
    (void)source;
    (void)expectedLengthSeconds;
#if KRAKEN_HAS_MINIZIP
    return makeError("ZIP stem bundles require libsndfile support for decoding.");
#else
    return makeError("ZIP stem bundles are unavailable because this build lacks minizip support.");
#endif
#else
#if !KRAKEN_HAS_MINIZIP
    (void)source;
    (void)expectedLengthSeconds;
    return makeError("ZIP stem bundles are unavailable because this build lacks minizip support.");
#else
    namespace fs = std::filesystem;

    if (!fs::exists(source.archive)) {
        return makeError("Stem archive does not exist: " + source.archive.string());
    }

    unzFile archive = unzOpen64(source.archive.string().c_str());
    if (!archive) {
        return makeError("Unable to open stem archive: " + source.archive.string());
    }

    std::vector<SndfileHandle> handles;
    double referenceRate = 0.0;
    bool sampleMismatch = false;

    int status = unzGoToFirstFile(archive);
    while (status == UNZ_OK) {
        unz_file_info64 info{};
        status = unzGetCurrentFileInfo64(archive, &info, nullptr, 0, nullptr, 0, nullptr, 0);
        if (status != UNZ_OK) {
            unzClose(archive);
            return makeError("Failed to query entry metadata in archive: " + source.archive.string());
        }

        std::vector<char> nameBuffer(static_cast<std::size_t>(info.size_filename) + 1, '\0');
        status = unzGetCurrentFileInfo64(archive, &info, nameBuffer.data(), static_cast<unsigned int>(nameBuffer.size()), nullptr, 0, nullptr, 0);
        if (status != UNZ_OK) {
            unzClose(archive);
            return makeError("Failed to read entry name in archive: " + source.archive.string());
        }

        std::string entryName{nameBuffer.data()};
        const std::filesystem::path entryPath(entryName);
        if (!hasExtension(entryPath, {".wav", ".wave"})) {
            status = unzGoToNextFile(archive);
            if (handle.reset) {
                handle.reset();
            }
            if (handle.memory) {
                handle.memory->position = 0;
            }
            m_finished[i] = false;
            m_framesRead[i] = 0;
        }
        m_valid = true;
        m_error.clear();
    }

    [[nodiscard]] bool valid() const noexcept { return m_valid; }
    [[nodiscard]] const std::string& error() const noexcept { return m_error; }

private:
    std::vector<SndfileHandle> m_handles;
    std::vector<bool> m_finished;
    std::vector<sf_count_t> m_framesRead;
    std::vector<ClipChannelMetadata> m_metadata;
    std::vector<float> m_interleavedScratch;
    std::size_t m_chunkFrames{};
    double m_sampleRate{};
    std::size_t m_totalChannels{};
    std::optional<std::size_t> m_totalFrames;
    bool m_valid{false};
    std::string m_error;
};

class Mp3ClipStream : public ClipStream {
public:
    Mp3ClipStream(std::filesystem::path file,
                  std::vector<ClipChannelMetadata> providedMetadata,
                  std::size_t chunkFrames)
        : m_path(std::move(file)),
          m_providedMetadata(std::move(providedMetadata)),
          m_chunkFrames(chunkFrames) {}

    ~Mp3ClipStream() override {
        if (m_open) {
            mp3dec_ex_close(&m_decoder);
        }
    }

    bool initialize(std::string& error) {
        if (!std::filesystem::exists(m_path)) {
            error = "Missing MP3 file at " + m_path.string();
            return false;
        }

        if (mp3dec_ex_open_file(&m_decoder, m_path.string().c_str(), MP3D_SEEK_TO_SAMPLE)) {
            error = "minimp3 failed to open " + m_path.string();
            return false;
        }

        m_open = true;
        m_sampleRate = static_cast<double>(m_decoder.info.hz);
        m_channels = static_cast<std::size_t>(m_decoder.info.channels);
        if (m_channels == 0) {
            error = "MP3 stream contained no channels";
            return false;
        }

        m_metadata = ensureMetadata(m_providedMetadata, m_channels);
        if (m_decoder.samples > 0) {
            m_totalFrames = static_cast<std::size_t>(m_decoder.samples / m_channels);
        }
        return true;
    }

    [[nodiscard]] double sampleRate() const noexcept override { return m_sampleRate; }
    [[nodiscard]] std::size_t channelCount() const noexcept override { return m_channels; }
    [[nodiscard]] const std::vector<ClipChannelMetadata>& metadata() const noexcept override { return m_metadata; }
    [[nodiscard]] std::size_t chunkSize() const noexcept override { return m_chunkFrames; }
    [[nodiscard]] std::optional<std::size_t> totalFrames() const noexcept override { return m_totalFrames; }

    bool next(ClipStreamChunk& chunk) override {
        if (!m_open || m_eof) {
            return false;
        }

        const std::size_t requiredSamples = m_channels * m_chunkFrames;
        if (m_decodeScratch.size() < requiredSamples) {
            m_decodeScratch.resize(requiredSamples);
        }

        const std::size_t samplesRead = mp3dec_ex_read(&m_decoder, m_decodeScratch.data(), requiredSamples);
        if (samplesRead == 0) {
            m_eof = true;
            return false;
        }

        const std::size_t frames = samplesRead / m_channels;
        chunk.frames = frames;
        chunk.channels.resize(m_channels);
        for (std::size_t ch = 0; ch < m_channels; ++ch) {
            auto& dest = chunk.channels[ch];
            dest.resize(frames);
            for (std::size_t frame = 0; frame < frames; ++frame) {
                const auto index = frame * m_channels + ch;
                dest[frame] = m_decodeScratch[index];
            }
        }

        if (frames < m_chunkFrames) {
            m_eof = true;
        }
        return true;
    }

    void reset() override {
        if (!m_open) {
            return;
        }
        mp3dec_ex_seek(&m_decoder, 0);
        m_eof = false;
    }

private:
    std::filesystem::path m_path;
    mp3dec_ex_t m_decoder{};
    bool m_open{false};
    bool m_eof{false};
    std::vector<float> m_decodeScratch;
    std::vector<ClipChannelMetadata> m_providedMetadata;
    std::vector<ClipChannelMetadata> m_metadata;
    std::size_t m_chunkFrames{};
    double m_sampleRate{};
    std::size_t m_channels{};
    std::optional<std::size_t> m_totalFrames;
};

std::shared_ptr<ClipStream> buildAggregateStream(std::vector<SndfileHandle> handles,
                                                 std::vector<ClipChannelMetadata> metadata,
                                                 ClipLoader::Result& result) {
    if (handles.empty()) {
        result.error = "No audio stems available";
        return nullptr;
    }

    const int referenceRate = handles.front().info.samplerate;
    bool rateMismatch = false;
    for (std::size_t i = 1; i < handles.size(); ++i) {
        if (handles[i].info.samplerate != referenceRate) {
            rateMismatch = true;
            break;
        }
    }
    if (rateMismatch) {
        result.warnings.emplace_back("Stem sample rate mismatch detected");
    }

    const sf_count_t referenceFrames = handles.front().info.frames;
    bool lengthMismatch = false;
    if (referenceFrames > 0) {
        for (std::size_t i = 1; i < handles.size(); ++i) {
            if (handles[i].info.frames > 0 && handles[i].info.frames != referenceFrames) {
                lengthMismatch = true;
                break;
            }
        }
    }
    if (lengthMismatch) {
        result.warnings.emplace_back("Stem length mismatch detected");
    }

    std::size_t totalChannels = 0;
    for (const auto& handle : handles) {
        totalChannels += static_cast<std::size_t>(handle.info.channels);
    }
    if (metadata.size() < totalChannels) {
        metadata = ensureMetadata(metadata, totalChannels);
    }

    auto stream = std::make_shared<AggregateSndfileStream>(std::move(handles), std::move(metadata),
                                                           kDefaultChunkFrames);
    if (!stream->valid()) {
        result.error = stream->error();
        return nullptr;
    }
    return stream;
}

std::shared_ptr<ClipStream> loadZipBundle(const ZipClipSource& zip, ClipLoader::Result& result) {
    if (!std::filesystem::exists(zip.archive)) {
        result.error = "Missing stem archive at " + zip.archive.string();
        return nullptr;
    }

    unzFile archive = unzOpen64(zip.archive.string().c_str());
    if (!archive) {
        result.error = "Unable to open zip archive " + zip.archive.string();
        return nullptr;
    }

    auto closeArchive = [&]() { unzClose(archive); };

    if (unzGoToFirstFile(archive) != UNZ_OK) {
        closeArchive();
        result.error = "Zip archive contains no entries";
        return nullptr;
    }

    std::vector<SndfileHandle> handles;
    std::vector<ClipChannelMetadata> metadata;
    std::size_t metadataCursor = 0;

    do {
        unz_file_info64 fileInfo{};
        std::array<char, 512> fileName{};
        if (unzGetCurrentFileInfo64(archive, &fileInfo, fileName.data(), fileName.size(), nullptr, 0, nullptr, 0) !=
            UNZ_OK) {
            result.warnings.emplace_back("Unable to read zip entry metadata");
            continue;
        }

        const std::filesystem::path entryPath{fileName.data()};
        if (!hasExtension(entryPath, ".wav") && !hasExtension(entryPath, ".wave")) {
            continue;
        }

        if (unzOpenCurrentFile(archive) != UNZ_OK) {
            unzClose(archive);
            return makeError("Failed to extract entry '" + entryName + "' from archive: " + source.archive.string());
        }

        std::vector<std::uint8_t> data;
        data.resize(static_cast<std::size_t>(info.uncompressed_size));
        std::size_t offset = 0;
        while (offset < data.size()) {
            const auto remaining = static_cast<unsigned int>(data.size() - offset);
            const int read = unzReadCurrentFile(archive, data.data() + offset, remaining);
            if (read < 0) {
                unzCloseCurrentFile(archive);
                unzClose(archive);
                return makeError("Error while reading entry '" + entryName + "' from archive: " + source.archive.string());
            }
            if (read == 0) {
                break;
            }
            offset += static_cast<std::size_t>(read);
        }
        unzCloseCurrentFile(archive);
        data.resize(offset);
            result.warnings.emplace_back("Unable to open zip entry " + entryPath.string());
            continue;
        }

        std::vector<std::uint8_t> data;
        data.reserve(static_cast<std::size_t>(fileInfo.uncompressed_size));
        std::array<char, 64 * 1024> buffer{};
        int read = 0;
        while ((read = unzReadCurrentFile(archive, buffer.data(), static_cast<unsigned int>(buffer.size()))) > 0) {
            data.insert(data.end(), buffer.data(), buffer.data() + read);
        }
        unzCloseCurrentFile(archive);
        if (read < 0) {
            result.error = "Failed while reading zip entry " + entryPath.string();
            closeArchive();
            return nullptr;
        }

        auto memory = std::make_shared<MemoryBuffer>();
        memory->bytes = std::move(data);
        memory->position = 0;

        SF_INFO sndInfo{};
        SNDFILE* snd = sf_open_virtual(&kMemoryIO, SFM_READ, &sndInfo, memory.get());
        if (!snd) {
            unzClose(archive);
            return makeError("Failed to decode entry '" + entryName + "' from archive: " + std::string(sf_strerror(nullptr)));
        }

        if (referenceRate == 0.0) {
            referenceRate = static_cast<double>(sndInfo.samplerate);
        } else if (static_cast<double>(sndInfo.samplerate) != referenceRate) {
            sampleMismatch = true;
        }

        handles.emplace_back(source.archive / entryPath, snd, sndInfo, memory);

        status = unzGoToNextFile(archive);
    }

    unzClose(archive);

    if (handles.empty()) {
        return makeError("No WAV stems were found inside archive: " + source.archive.string());
    }

    std::sort(handles.begin(), handles.end(), [](const SndfileHandle& lhs, const SndfileHandle& rhs) {
        return lhs.path < rhs.path;
    });

    std::vector<std::string> warnings;
    if (sampleMismatch) {
        warnings.emplace_back("ZIP stems use different sample rates; playback will follow the first entry.");
    }

    auto metadata = source.channels;
    return makeStream(std::move(handles), std::move(metadata), expectedLengthSeconds, std::move(warnings));
#endif
#endif
}

ClipLoader::Result loadAudioFileSndfile(const AudioFileClipSource& source, double expectedLengthSeconds) {
#if KRAKEN_HAS_SNDFILE
    SF_INFO info{};
    SNDFILE* snd = sf_open(source.file.string().c_str(), SFM_READ, &info);
    if (!snd) {
        return makeError("Failed to open audio asset '" + source.file.string() + "': " + std::string(sf_strerror(nullptr)));
    }

    std::vector<SndfileHandle> handles;
    handles.emplace_back(source.file, snd, info);

    auto metadata = source.channels;
    return makeStream(std::move(handles), std::move(metadata), expectedLengthSeconds, {});
#else
    (void)source;
    (void)expectedLengthSeconds;
    return makeError("WAV/AIFF decoding is unavailable because libsndfile support is disabled at build time.");
#endif
}

#if KRAKEN_HAS_MINIMP3

class Minimp3ClipStream : public ClipStream {
public:
    struct DecoderDeleter {
        void operator()(mp3dec_ex_t* decoder) const noexcept {
            if (decoder) {
                mp3dec_ex_close(decoder);
                delete decoder;
            }
        }
    };

    Minimp3ClipStream(std::unique_ptr<mp3dec_ex_t, DecoderDeleter> decoder,
                      std::vector<ClipChannelMetadata> metadata,
                      std::size_t chunkFrames)
        : m_decoder(std::move(decoder)),
          m_metadata(std::move(metadata)),
          m_chunkFrames(chunkFrames) {
        if (!m_decoder) {
            return;
        }

        if (m_decoder->channels > 0) {
            m_channels = static_cast<std::size_t>(m_decoder->channels);
        } else if (m_decoder->info.channels > 0) {
            m_channels = static_cast<std::size_t>(m_decoder->info.channels);
        }

        if (m_decoder->hz > 0) {
            m_sampleRate = static_cast<double>(m_decoder->hz);
        } else if (m_decoder->info.hz > 0) {
            m_sampleRate = static_cast<double>(m_decoder->info.hz);
        }

        if (m_channels == 0) {
            return;
        }

        if (m_metadata.size() < m_channels) {
            m_metadata = ensureMetadata(m_metadata, m_channels);
        }

        if (m_decoder->samples > 0) {
            const auto totalSamples = static_cast<std::size_t>(m_decoder->samples);
            m_totalFrames = totalSamples / m_channels;
        }

        m_valid = true;
    }

    [[nodiscard]] double sampleRate() const noexcept override { return m_sampleRate; }
    [[nodiscard]] std::size_t channelCount() const noexcept override { return m_channels; }
    [[nodiscard]] const std::vector<ClipChannelMetadata>& metadata() const noexcept override { return m_metadata; }
    [[nodiscard]] std::size_t chunkSize() const noexcept override { return m_chunkFrames; }
    [[nodiscard]] std::optional<std::size_t> totalFrames() const noexcept override { return m_totalFrames; }

    bool next(ClipStreamChunk& chunk) override {
        if (!m_valid || m_channels == 0) {
            return false;
        }

        chunk.channels.assign(m_channels, {});
        chunk.frames = 0;

        const std::size_t samplesToRead = m_channels * m_chunkFrames;
        if (samplesToRead == 0) {
            return false;
        }

        m_interleaved.resize(samplesToRead);
        const std::size_t samplesRead = mp3dec_ex_read(m_decoder.get(), m_interleaved.data(), samplesToRead);
        if (samplesRead == 0) {
            return false;
        }

        const std::size_t frames = samplesRead / m_channels;
        if (frames == 0) {
            return false;
        }

        for (std::size_t ch = 0; ch < m_channels; ++ch) {
            auto& dest = chunk.channels[ch];
            dest.resize(frames);
            for (std::size_t frame = 0; frame < frames; ++frame) {
                dest[frame] = m_interleaved[frame * m_channels + ch];
            }
        }

        chunk.frames = frames;
        return true;
    }

    void reset() override {
        if (m_decoder) {
            mp3dec_ex_seek(m_decoder.get(), 0);
        }
    }

private:
    std::unique_ptr<mp3dec_ex_t, DecoderDeleter> m_decoder;
    std::vector<ClipChannelMetadata> m_metadata;
    std::vector<float> m_interleaved;
    std::size_t m_chunkFrames{};
    std::size_t m_channels{};
    double m_sampleRate{};
    std::optional<std::size_t> m_totalFrames;
    bool m_valid{false};
};

ClipLoader::Result makeMp3Stream(std::unique_ptr<mp3dec_ex_t, Minimp3ClipStream::DecoderDeleter> decoder,
                                 std::vector<ClipChannelMetadata> metadata,
                                 double expectedLengthSeconds) {
    auto stream = std::make_shared<Minimp3ClipStream>(std::move(decoder), std::move(metadata), kDefaultChunkFrames);
    if (!stream || stream->channelCount() == 0) {
        return makeError("Failed to initialise the MP3 decoder stream.");
    }

    ClipBuffer buffer;
    buffer.stream = std::move(stream);

    ClipLoader::Result result;
    result.buffer = std::move(buffer);

    if (result.buffer && result.buffer->stream) {
        const auto totalFrames = result.buffer->stream->totalFrames();
        if (totalFrames && result.buffer->stream->sampleRate() > 0.0 && expectedLengthSeconds > 0.0) {
            const double durationSeconds = static_cast<double>(*totalFrames) / result.buffer->stream->sampleRate();
            if (std::abs(durationSeconds - expectedLengthSeconds) > 0.5) {
                std::ostringstream oss;
                oss << "Expected clip length of " << expectedLengthSeconds
                    << "s but decoded " << durationSeconds << "s of audio.";
                result.warnings.push_back(oss.str());
            }
        }
    }

    return result;
}

ClipLoader::Result loadMp3File(const AudioFileClipSource& source, double expectedLengthSeconds) {
    namespace fs = std::filesystem;

    if (!fs::exists(source.file)) {
        return makeError("Audio asset does not exist: " + source.file.string());
    }

    auto decoder = std::unique_ptr<mp3dec_ex_t, Minimp3ClipStream::DecoderDeleter>(new mp3dec_ex_t{});
    const int status = mp3dec_ex_open_file(decoder.get(), source.file.string().c_str(), MP3D_SEEK_TO_SAMPLE);
    if (status != 0) {
        return makeError("Failed to open MP3 asset '" + source.file.string() + "'.");
    }

    auto metadata = source.channels;
    return makeMp3Stream(std::move(decoder), std::move(metadata), expectedLengthSeconds);
}

#else

ClipLoader::Result loadMp3File(const AudioFileClipSource& source, double) {
    namespace fs = std::filesystem;
    if (!fs::exists(source.file)) {
        return makeError("Audio asset does not exist: " + source.file.string());
    }
    return makeError("MP3 decoding is unavailable because minimp3 support is disabled at build time.");
}

#endif // KRAKEN_HAS_MINIMP3

ClipLoader::Result loadAudioFile(const AudioFileClipSource& source, double expectedLengthSeconds) {
    if (source.isMp3) {
        return loadMp3File(source, expectedLengthSeconds);
    }
    return loadAudioFileSndfile(source, expectedLengthSeconds);
}

ClipLoader::Result dispatchLoad(const ZipClipSource& source, double expectedLengthSeconds) {
    return loadZipArchive(source, expectedLengthSeconds);
}

ClipLoader::Result dispatchLoad(const WavDirectoryClipSource& source, double expectedLengthSeconds) {
    return loadWavDirectory(source, expectedLengthSeconds);
}

ClipLoader::Result dispatchLoad(const AudioFileClipSource& source, double expectedLengthSeconds) {
    return loadAudioFile(source, expectedLengthSeconds);
}

} // namespace

ClipLoader::Result ClipLoader::load(const ClipSource& source, double expectedLengthSeconds) const {
    return std::visit([&](const auto& specific) { return dispatchLoad(specific, expectedLengthSeconds); }, source.source);
        SF_INFO info{};
        SNDFILE* snd = sf_open_virtual(&kMemoryIO, SFM_READ, &info, memory.get());
        if (!snd) {
            result.error = "libsndfile failed to open " + entryPath.string();
            closeArchive();
            return nullptr;
        }

        SndfileHandle handle;
        handle.file = snd;
        handle.info = info;
        handle.memory = memory;
        handle.reset = [snd, memory]() {
            memory->position = 0;
            return sf_seek(snd, 0, SEEK_SET) >= 0;
        };
        handle.close = [snd]() { sf_close(snd); };

        handles.push_back(std::move(handle));

        const auto channelCount = static_cast<std::size_t>(info.channels);
        auto channelMetadata = ensureMetadata(zip.channels, channelCount, metadataCursor);
        metadata.insert(metadata.end(), channelMetadata.begin(), channelMetadata.end());
        metadataCursor += channelCount;
    } while (unzGoToNextFile(archive) == UNZ_OK);

    closeArchive();

    if (handles.empty()) {
        result.error = "No WAV stems found in " + zip.archive.string();
        return nullptr;
    }

    return buildAggregateStream(std::move(handles), std::move(metadata), result);
}

std::shared_ptr<ClipStream> loadWavDirectory(const WavDirectoryClipSource& directory,
                                             ClipLoader::Result& result) {
    if (!std::filesystem::exists(directory.directory) ||
        !std::filesystem::is_directory(directory.directory)) {
        result.error = "Stem directory missing at " + directory.directory.string();
        return nullptr;
    }

    std::vector<std::filesystem::path> wavFiles;
    for (const auto& entry : std::filesystem::directory_iterator(directory.directory)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (hasExtension(entry.path(), ".wav") || hasExtension(entry.path(), ".wave")) {
            wavFiles.push_back(entry.path());
        }
    }

    std::sort(wavFiles.begin(), wavFiles.end());

    if (wavFiles.empty()) {
        result.error = "No WAV stems found in " + directory.directory.string();
        return nullptr;
    }

    std::vector<SndfileHandle> handles;
    std::vector<ClipChannelMetadata> metadata;
    std::size_t metadataCursor = 0;

    for (const auto& wav : wavFiles) {
        SF_INFO info{};
        SNDFILE* snd = sf_open(wav.string().c_str(), SFM_READ, &info);
        if (!snd) {
            result.error = "libsndfile failed to open " + wav.string();
            return nullptr;
        }

        SndfileHandle handle;
        handle.file = snd;
        handle.info = info;
        handle.reset = [snd]() { return sf_seek(snd, 0, SEEK_SET) >= 0; };
        handle.close = [snd]() { sf_close(snd); };

        handles.push_back(std::move(handle));

        const auto channelCount = static_cast<std::size_t>(info.channels);
        auto channelMetadata = ensureMetadata(directory.channels, channelCount, metadataCursor);
        metadata.insert(metadata.end(), channelMetadata.begin(), channelMetadata.end());
        metadataCursor += channelCount;
    }

    return buildAggregateStream(std::move(handles), std::move(metadata), result);
}

std::shared_ptr<ClipStream> loadAudioFile(const AudioFileClipSource& file, ClipLoader::Result& result) {
    if (file.isMp3 || hasExtension(file.file, ".mp3")) {
        auto stream = std::make_shared<Mp3ClipStream>(file.file, file.channels, kDefaultChunkFrames);
        if (!stream->initialize(result.error)) {
            return nullptr;
        }
        result.buffer = ClipBuffer{stream};
        return stream;
    }

    if (hasExtension(file.file, ".wav") || hasExtension(file.file, ".wave")) {
        SF_INFO info{};
        SNDFILE* snd = sf_open(file.file.string().c_str(), SFM_READ, &info);
        if (!snd) {
            result.error = "libsndfile failed to open " + file.file.string();
            return nullptr;
        }

        SndfileHandle handle;
        handle.file = snd;
        handle.info = info;
        handle.reset = [snd]() { return sf_seek(snd, 0, SEEK_SET) >= 0; };
        handle.close = [snd]() { sf_close(snd); };

        std::vector<SndfileHandle> handles;
        handles.push_back(std::move(handle));

        const auto channelCount = static_cast<std::size_t>(info.channels);
        auto metadata = ensureMetadata(file.channels, channelCount);
        return buildAggregateStream(std::move(handles), std::move(metadata), result);
    }

    result.error = "Unsupported audio file type for " + file.file.string();
    return nullptr;
}

} // namespace

ClipLoader::Result ClipLoader::load(const ClipSource& source, double expectedLengthSeconds) const {
    Result result;

    std::shared_ptr<ClipStream> stream;
    switch (clipSourceType(source)) {
        case ClipSourceType::ZipArchive:
            stream = loadZipBundle(std::get<ZipClipSource>(source.source), result);
            break;
        case ClipSourceType::WavDirectory:
            stream = loadWavDirectory(std::get<WavDirectoryClipSource>(source.source), result);
            break;
        case ClipSourceType::AudioFile:
            stream = loadAudioFile(std::get<AudioFileClipSource>(source.source), result);
            break;
    }

    if (!stream) {
        return result;
    }

    if (!result.buffer.has_value()) {
        ClipBuffer buffer;
        buffer.stream = std::move(stream);
        result.buffer = std::move(buffer);
    }

    if (result.buffer && result.buffer->stream) {
        const auto totalFrames = result.buffer->stream->totalFrames();
        const double sampleRate = result.buffer->stream->sampleRate();
        if (totalFrames.has_value() && expectedLengthSeconds > 0.0 && sampleRate > 0.0) {
            const auto expectedFrames = static_cast<std::size_t>(expectedLengthSeconds * sampleRate);
            const auto actualFrames = *totalFrames;
            const double delta = std::fabs(static_cast<double>(actualFrames) -
                                           static_cast<double>(expectedFrames));
            if (delta > sampleRate * 0.05) {
                result.warnings.emplace_back("Decoded clip length differs from scheduled length");
            }
        }
    }

    return result;
}

} // namespace kraken::audio
