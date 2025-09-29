#include "ai/AIAssistant.h"
#include "audio/StemResolver.h"

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

namespace {

void setEnvVar(const std::string& key, const std::string& value) {
#ifdef _WIN32
    _putenv_s(key.c_str(), value.c_str());
#else
    setenv(key.c_str(), value.c_str(), 1);
#endif
}

void unsetEnvVar(const std::string& key) {
#ifdef _WIN32
    _putenv_s(key.c_str(), "");
#else
    unsetenv(key.c_str());
#endif
}

class EnvOverride {
public:
    explicit EnvOverride(std::string key)
        : m_key(std::move(key)) {
        if (const char* existing = std::getenv(m_key.c_str())) {
            m_previous = existing;
        }
    }

    void set(const std::string& value) { setEnvVar(m_key, value); }

    void clear() { unsetEnvVar(m_key); }

    ~EnvOverride() {
        if (m_previous.has_value()) {
            setEnvVar(m_key, *m_previous);
        } else {
            unsetEnvVar(m_key);
        }
    }

private:
    std::string m_key;
    std::optional<std::string> m_previous;
};

void runStemResolverSmokeTests() {
    namespace fs = std::filesystem;

    const auto uniqueSuffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto tempRoot = fs::temp_directory_path() / ("kraken_manifest_test_" + uniqueSuffix);
    fs::create_directories(tempRoot);

    const auto zipFile = tempRoot / "zip_track.zip";
    std::ofstream(zipFile) << "zip";

    const auto wavDir = tempRoot / "wav_track";
    fs::create_directories(wavDir);
    for (int i = 0; i < 4; ++i) {
        std::ofstream(wavDir / ("stem_" + std::to_string(i) + ".wav")) << "wav";
    }

    const auto audioFile = tempRoot / "mix_render.wav";
    std::ofstream(audioFile) << "audio";

    const auto manifestPath = tempRoot / "stems_manifest.json";
    {
        std::ofstream manifest(manifestPath);
        manifest << "{\n"
                 << "  \"ZIP_TRACK\": { \"type\": \"zip\", \"path\": \"" << zipFile.filename().string()
                 << "\" },\n"
                 << "  \"DIR_TRACK\": { \"type\": \"wav_dir\", \"path\": \"" << wavDir.filename().string()
                 << "\" },\n"
                 << "  \"FILE_TRACK\": { \"type\": \"audio_file\", \"path\": \"" << audioFile.filename().string()
                 << "\" }\n"
                 << "}\n";
    }

    EnvOverride env("KRAKEN_STEM_MANIFEST");
    env.set(manifestPath.string());

    const auto zipSource = kraken::audio::resolveClipSource("ZIP_TRACK");
    assert(zipSource.type == kraken::audio::ClipSourceType::ZipArchive);
    assert(std::filesystem::equivalent(zipSource.path, zipFile));

    const auto dirSource = kraken::audio::resolveClipSource("DIR_TRACK");
    assert(dirSource.type == kraken::audio::ClipSourceType::WavDirectory);
    assert(std::filesystem::equivalent(dirSource.path, wavDir));

    const auto fileSource = kraken::audio::resolveClipSource("FILE_TRACK");
    assert(fileSource.type == kraken::audio::ClipSourceType::AudioFile);
    assert(std::filesystem::equivalent(fileSource.path, audioFile));

    const auto fallbackSource = kraken::audio::resolveClipSource("UNKNOWN_TRACK");
    assert(fallbackSource.type == kraken::audio::ClipSourceType::ZipArchive);
    assert(fallbackSource.path == std::filesystem::path{"stems"} / "UNKNOWN_TRACK.zip");

    const auto overrideZip = tempRoot / "override.zip";
    std::ofstream(overrideZip) << "zip";
    const auto overrideManifest = tempRoot / "override_manifest.json";
    {
        std::ofstream manifest(overrideManifest);
        manifest << "{\n"
                 << "  \"ZIP_TRACK\": { \"type\": \"zip\", \"path\": \"" << overrideZip.filename().string() << "\" }\n"
                 << "}\n";
    }

    env.set(overrideManifest.string());
    const auto overriddenSource = kraken::audio::resolveClipSource("ZIP_TRACK");
    assert(std::filesystem::equivalent(overriddenSource.path, overrideZip));

    env.clear();
    fs::remove_all(tempRoot);
}

} // namespace

int main() {
    kraken::ai::AIAssistant assistant;
    assistant.setMode(kraken::ai::AIMode::CollaborativeRecommend);
    const auto recs = assistant.recommendTracks("groove", 5);
    assert(!recs.empty());
    std::cout << "Received " << recs.size() << " groove recommendations\n";

    runStemResolverSmokeTests();
    return 0;
}
