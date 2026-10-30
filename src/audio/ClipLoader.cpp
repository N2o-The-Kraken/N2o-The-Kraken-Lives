#include "audio/ClipLoader.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
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
#if KRAKEN_HAS_MINIMP3
#define MINIMP3_FLOAT_OUTPUT
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

std::vector<ClipChannelMetadata> ensureMetadata(const std::vector<ClipChannelMetadata>& provided,
                                                std::size_t required) {
    std::vector<ClipChannelMetadata> result;
    result.reserve(required);

    static constexpr std::array<std::string_view, 4> kDefaults{"Drums", "Bass", "Music", "Vocals"};

    for (std::size_t index = 0; index < required; ++index) {
        if (index < provided.size()) {
            result.push_back(provided[index]);
        } else {
            ClipChannelMetadata metadata;
            metadata.id = "channel_" + std::to_string(index + 1);
            if (index < kDefaults.size()) {
                metadata.description = std::string(kDefaults[index]);
            } else {
                metadata.description = "Stem " + std::to_string(index + 1);
            }
            result.push_back(std::move(metadata));
        }
    }

    return result;
}

class MemoryClipStream : public ClipStream {
public:
    MemoryClipStream(double sampleRate,
                     std::vector<std::vector<float>> samples,
                     std::vector<ClipChannelMetadata> metadata,
                     std::size_t chunkFrames)
        : m_sampleRate(sampleRate),
          m_samples(std::move(samples)),
          m_metadata(std::move(metadata)),
          m_chunkFrames(chunkFrames) {
        if (!m_samples.empty()) {
            m_totalFrames = m_samples.front().size();
        }
    }

    [[nodiscard]] double sampleRate() const noexcept override { return m_sampleRate; }
    [[nodiscard]] std::size_t channelCount() const noexcept override { return m_samples.size(); }
    [[nodiscard]] const std::vector<ClipChannelMetadata>& metadata() const noexcept override { return m_metadata; }
    [[nodiscard]] std::size_t chunkSize() const noexcept override { return m_chunkFrames; }

    [[nodiscard]] std::optional<std::size_t> totalFrames() const noexcept override {
        return m_totalFrames;
    }

    bool next(ClipStreamChunk& chunk) override {
        if (m_position >= m_totalFrames) {
            return false;
        }

        const std::size_t framesRemaining = m_totalFrames - m_position;
        const std::size_t frames = std::min(m_chunkFrames, framesRemaining);

        chunk.frames = frames;
        chunk.channels.resize(m_samples.size());
        for (std::size_t channel = 0; channel < m_samples.size(); ++channel) {
            const auto& data = m_samples[channel];
            chunk.channels[channel].assign(data.begin() + static_cast<std::ptrdiff_t>(m_position),
                                           data.begin() + static_cast<std::ptrdiff_t>(m_position + frames));
        }

        m_position += frames;
        return true;
    }

    void reset() override { m_position = 0; }

private:
    double m_sampleRate{44100.0};
    std::vector<std::vector<float>> m_samples;
    std::vector<ClipChannelMetadata> m_metadata;
    std::size_t m_chunkFrames{0};
    std::size_t m_totalFrames{0};
    std::size_t m_position{0};
};

#if KRAKEN_HAS_SNDFILE
ClipLoader::Result loadWithSndfile(const AudioFileClipSource& file, double expectedLengthSeconds) {
    SF_INFO info{};
    info.format = 0;

    std::unique_ptr<SNDFILE, decltype(&sf_close)> handle(sf_open(file.file.string().c_str(), SFM_READ, &info), &sf_close);
    if (!handle) {
        return makeError("Failed to open audio file: " + file.file.string());
    }

    if (info.frames <= 0 || info.channels <= 0) {
        return makeError("Audio file has no frames: " + file.file.string());
    }

    const auto metadata = ensureMetadata(file.channels, static_cast<std::size_t>(info.channels));
    std::vector<std::vector<float>> channels(static_cast<std::size_t>(info.channels));
    for (auto& channel : channels) {
        channel.resize(static_cast<std::size_t>(info.frames));
    }

    std::vector<float> interleaved(static_cast<std::size_t>(info.frames * info.channels));
    const sf_count_t read = sf_readf_float(handle.get(), interleaved.data(), info.frames);
    if (read != info.frames) {
        return makeError("Unable to read entire file: " + file.file.string());
    }

    for (sf_count_t frame = 0; frame < info.frames; ++frame) {
        for (int channel = 0; channel < info.channels; ++channel) {
            const std::size_t destIndex = static_cast<std::size_t>(frame);
            const std::size_t sourceIndex = static_cast<std::size_t>(frame * info.channels + channel);
            channels[static_cast<std::size_t>(channel)][destIndex] = interleaved[sourceIndex];
        }
    }

    ClipLoader::Result result;
    const double lengthSeconds = static_cast<double>(info.frames) / static_cast<double>(info.samplerate);
    if (expectedLengthSeconds > 0.0 && std::abs(lengthSeconds - expectedLengthSeconds) > 0.25) {
        result.warnings.push_back("Expected clip length " + std::to_string(expectedLengthSeconds) +
                                   "s but decoded length is " + std::to_string(lengthSeconds) + "s");
    }

    result.buffer = ClipBuffer{.stream = std::make_shared<MemoryClipStream>(
                        static_cast<double>(info.samplerate), std::move(channels), metadata, kDefaultChunkFrames)};
    return result;
}
#endif

#if KRAKEN_HAS_MINIMP3
ClipLoader::Result loadWithMinimp3(const AudioFileClipSource& file, double expectedLengthSeconds) {
    mp3dec_ex_t decoder{};
    if (mp3dec_ex_open_file(&decoder, file.file.string().c_str(), MP3D_SEEK_TO_SAMPLE) != 0) {
        return makeError("Failed to open MP3 file: " + file.file.string());
    }

    struct DecoderCloser {
        mp3dec_ex_t* decoder{nullptr};
        ~DecoderCloser() {
            if (decoder) {
                mp3dec_ex_close(decoder);
            }
        }
    } closer{&decoder};

    if (decoder.info.channels <= 0 || decoder.info.hz <= 0) {
        return makeError("MP3 file reports invalid format: " + file.file.string());
    }

    const std::size_t channels = static_cast<std::size_t>(decoder.info.channels);
    auto metadata = ensureMetadata(file.channels, channels);

    const std::size_t chunkSamples = channels * kDefaultChunkFrames;
    std::vector<float> chunk(chunkSamples);
    std::vector<float> interleaved;
    interleaved.reserve(static_cast<std::size_t>(decoder.samples));

    while (true) {
        const std::size_t read = mp3dec_ex_read(&decoder, chunk.data(), chunkSamples);
        if (read == 0) {
            break;
        }
        interleaved.insert(interleaved.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(read));
    }

    if (interleaved.empty()) {
        return makeError("MP3 file contained no audio data: " + file.file.string());
    }

    const std::size_t totalSamples = interleaved.size();
    const std::size_t frames = totalSamples / channels;
    if (frames == 0) {
        return makeError("MP3 file contained incomplete channel data: " + file.file.string());
    }

    const std::size_t alignedSamples = frames * channels;
    if (alignedSamples != totalSamples) {
        interleaved.resize(alignedSamples);
    }

    std::vector<std::vector<float>> channelData(channels, std::vector<float>(frames));
    for (std::size_t frame = 0; frame < frames; ++frame) {
        for (std::size_t channel = 0; channel < channels; ++channel) {
            const std::size_t index = frame * channels + channel;
            channelData[channel][frame] = interleaved[index];
        }
    }

    ClipLoader::Result result;
    const double sampleRate = static_cast<double>(decoder.info.hz);
    if (expectedLengthSeconds > 0.0 && sampleRate > 0.0) {
        const double decodedLength = static_cast<double>(frames) / sampleRate;
        if (std::abs(decodedLength - expectedLengthSeconds) > 0.25) {
            result.warnings.push_back("Expected clip length " + std::to_string(expectedLengthSeconds) +
                                       "s but decoded length is " + std::to_string(decodedLength) + "s");
        }
    }

    result.buffer = ClipBuffer{.stream = std::make_shared<MemoryClipStream>(
                        sampleRate, std::move(channelData), std::move(metadata), kDefaultChunkFrames)};
    return result;
}
#endif

ClipLoader::Result loadAudioFile(const AudioFileClipSource& file, double expectedLengthSeconds) {
#if KRAKEN_HAS_MINIMP3
    if (file.isMp3) {
        return loadWithMinimp3(file, expectedLengthSeconds);
    }
#else
    if (file.isMp3) {
        (void)expectedLengthSeconds;
        return makeError("MP3 decoding requires minimp3 support at build time.");
    }
#endif

#if KRAKEN_HAS_SNDFILE
    return loadWithSndfile(file, expectedLengthSeconds);
#else
    (void)expectedLengthSeconds;
    (void)file;
    return makeError("Audio file decoding requires libsndfile support.");
#endif
}

ClipLoader::Result loadWavDirectory(const WavDirectoryClipSource& source) {
    return makeError("WAV directory clip sources are not implemented in this build.");
}

ClipLoader::Result loadZipArchive(const ZipClipSource& source) {
#if KRAKEN_HAS_MINIZIP
    (void)source;
    return makeError("ZIP archive loading is not yet implemented.");
#else
    (void)source;
    return makeError("ZIP archive support requires minizip at build time.");
#endif
}

} // namespace

ClipLoader::Result ClipLoader::load(const ClipSource& source, double expectedLengthSeconds) const {
    switch (clipSourceType(source)) {
        case ClipSourceType::AudioFile:
            return loadAudioFile(std::get<AudioFileClipSource>(source.source), expectedLengthSeconds);
        case ClipSourceType::WavDirectory:
            return loadWavDirectory(std::get<WavDirectoryClipSource>(source.source));
        case ClipSourceType::ZipArchive:
            return loadZipArchive(std::get<ZipClipSource>(source.source));
    }

    return makeError("Unknown clip source type.");
}

} // namespace kraken::audio
