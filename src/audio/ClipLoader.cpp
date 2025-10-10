#include "audio/ClipLoader.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

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
