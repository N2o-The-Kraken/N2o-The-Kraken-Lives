#include <chrono>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

#include "ai/AIAssistant.h"
#include "audio/Clip.h"
#include "audio/EffectChain.h"
#include "audio/Mixer.h"
#include "audio/StemResolver.h"
#include "audio/Timeline.h"
#include "controllers/ControllerManager.h"
#include "platform/JuceApp.h"
#include "platform/Platform.h"
#include "runtime/ModuleOrchestrator.h"

using namespace std::chrono_literals;

namespace {
kraken::audio::ClipSource demoStemSource(const std::string& trackId) {
    return kraken::audio::resolveClipSource(trackId);
}

std::string controllerName(kraken::controllers::ControllerType type) {
    switch (type) {
        case kraken::controllers::ControllerType::AkaiAPC64:
            return "Akai APC64";
        case kraken::controllers::ControllerType::NumarkPartyMix:
            return "Numark Party Mix";
        case kraken::controllers::ControllerType::NativeInstrumentsX1:
            return "NI Traktor X1";
    }
    return "Unknown";
}

class MixerBootstrapModule : public kraken::runtime::Module {
public:
    explicit MixerBootstrapModule(kraken::audio::Mixer& mixer) noexcept
        : m_mixer(mixer) {}

    std::string name() const override { return "MixerBootstrap"; }

    bool start(std::string* /*errorMessage*/) override {
        m_deckA = m_mixer.createChannel("Deck A");
        m_deckB = m_mixer.createChannel("Deck B");
        m_deckC = m_mixer.createChannel("Deck C");
        m_fxBus = m_mixer.createChannel("Macro FX Bus");
        m_ambient = m_mixer.createChannel("Ambient Pad");

        m_mixer.updateChannel(m_fxBus, {.volume = 0.65, .pan = 0.0, .sendLevels = {{"reverb", 0.8}}});
        m_mixer.updateChannel(m_ambient, {.volume = 0.55, .pan = -0.25, .sendLevels = {{"delay", 0.4}}});

        return true;
    }

    void shutdown() override {}

private:
    kraken::audio::Mixer& m_mixer;
    kraken::audio::Mixer::ChannelId m_deckA{};
    kraken::audio::Mixer::ChannelId m_deckB{};
    kraken::audio::Mixer::ChannelId m_deckC{};
    kraken::audio::Mixer::ChannelId m_fxBus{};
    kraken::audio::Mixer::ChannelId m_ambient{};
};

class TimelineBootstrapModule : public kraken::runtime::Module {
public:
    TimelineBootstrapModule(kraken::audio::Timeline& timeline, double tempo)
        : m_timeline(timeline), m_tempo(tempo) {}

    std::string name() const override { return "TimelineBootstrap"; }

    bool start(std::string* /*errorMessage*/) override {
        m_timeline.setTempo(m_tempo);
        m_deckATrack = m_timeline.createTrack("Deck A");
        m_deckBTrack = m_timeline.createTrack("Deck B");
        return true;
    }

    void shutdown() override {}

    [[nodiscard]] kraken::audio::Timeline::TrackId deckATrack() const { return m_deckATrack; }
    [[nodiscard]] kraken::audio::Timeline::TrackId deckBTrack() const { return m_deckBTrack; }

private:
    kraken::audio::Timeline& m_timeline;
    double m_tempo;
    kraken::audio::Timeline::TrackId m_deckATrack{};
    kraken::audio::Timeline::TrackId m_deckBTrack{};
};

class ControllerBootstrapModule : public kraken::runtime::Module {
public:
    ControllerBootstrapModule(kraken::controllers::ControllerManager& controllers,
                              kraken::ai::AIAssistant& assistant)
        : m_controllers(controllers), m_assistant(assistant) {}

    std::string name() const override { return "ControllerBootstrap"; }

    bool start(std::string* /*errorMessage*/) override {
        m_controllers.onMessage([&](kraken::controllers::ControllerType type,
                                    const kraken::controllers::ControlMessage& message) {
            std::cout << "Controller message from " << controllerName(type)
                      << ": " << message.control << " -> " << message.value << '\n';
            m_assistant.recordEvent({.trackId = message.control, .action = "tweak", .timestampSeconds = message.value});
        });

        m_controllers.addController(std::make_unique<kraken::controllers::AkaiAPC64>());
        m_controllers.addController(std::make_unique<kraken::controllers::NumarkPartyMix>());
        m_controllers.addController(std::make_unique<kraken::controllers::NativeInstrumentsX1>());
        return true;
    }

    void shutdown() override {}

private:
    kraken::controllers::ControllerManager& m_controllers;
    kraken::ai::AIAssistant& m_assistant;
};

class ClipSchedulingModule : public kraken::runtime::Module {
public:
    using ClipSourceFactory = std::function<kraken::audio::ClipSource(const std::string&)>;

    ClipSchedulingModule(kraken::audio::Timeline& timeline,
                         const TimelineBootstrapModule& timelineModule,
                         ClipSourceFactory clipSourceFactory,
                         std::shared_ptr<kraken::audio::Clip>& deckAClip,
                         std::shared_ptr<kraken::audio::Clip>& deckBClip)
        : m_timeline(timeline),
          m_timelineModule(timelineModule),
          m_clipSourceFactory(std::move(clipSourceFactory)),
          m_deckAClip(deckAClip),
          m_deckBClip(deckBClip) {}

    std::string name() const override { return "ClipScheduling"; }

    bool start(std::string* errorMessage) override {
        if (!m_clipSourceFactory) {
            if (errorMessage) {
                *errorMessage = "Clip source factory not configured";
            }
            return false;
        }

        m_deckAClip = std::make_shared<kraken::audio::Clip>(
            "Season1 Intro Groove", m_clipSourceFactory("S1_HGWSR_001"), 64.0);
        kraken::audio::ScheduledClip scA{
            .clip = m_deckAClip,
            .startBeat = 0.0,
            .durationBeats = 64.0,
        };
        const auto fxChainA = scA.effects.addEffect({
            .name = "Low-pass wash",
            .type = kraken::audio::EffectType::Filter,
            .parameters = {.values = {{"cutoff", 2000.0}, {"resonance", 0.5}}},
            .activationStartBeat = 60.0,
            .activationEndBeat = 64.0});
        scA.effects.linkEffects(fxChainA, "macro_downbeat_transition");
        m_timeline.scheduleClip(m_timelineModule.deckATrack(), std::move(scA));

        m_deckBClip = std::make_shared<kraken::audio::Clip>(
            "Season4 Perc Loop", m_clipSourceFactory("S4_HGWSR_042"), 32.0);
        kraken::audio::ScheduledClip scB{
            .clip = m_deckBClip,
            .startBeat = 32.0,
            .durationBeats = 32.0,
        };
        const auto fxChainB = scB.effects.addEffect({
            .name = "Granular riser",
            .type = kraken::audio::EffectType::Granular,
            .parameters = {.values = {{"density", 0.6}, {"spray", 0.2}}},
            .activationStartBeat = 56.0,
            .activationEndBeat = 64.0});
        scB.effects.linkEffects(fxChainB, "macro_downbeat_transition");
        m_timeline.scheduleClip(m_timelineModule.deckBTrack(), std::move(scB));

        return true;
    }

    void shutdown() override {
        if (m_deckAClip) {
            m_deckAClip->release();
            m_deckAClip.reset();
        }
        if (m_deckBClip) {
            m_deckBClip->release();
            m_deckBClip.reset();
        }
    }

private:
    kraken::audio::Timeline& m_timeline;
    const TimelineBootstrapModule& m_timelineModule;
    ClipSourceFactory m_clipSourceFactory;
    std::shared_ptr<kraken::audio::Clip>& m_deckAClip;
    std::shared_ptr<kraken::audio::Clip>& m_deckBClip;
};
}

int main(int argc, char** argv) {
    using kraken::ai::AIAssistant;
    using kraken::ai::AIMode;
    using kraken::audio::Clip;
    using kraken::audio::Mixer;
    using kraken::audio::ScheduledClip;
    using kraken::audio::Timeline;
    using kraken::controllers::ControllerManager;
    using kraken::platform::Platform;
    using kraken::platform::PlatformType;

    bool launchJuce = false;
    PlatformType requestedPlatform = PlatformType::Windows;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg{argv[i]};
        if (arg == "--juce") {
            launchJuce = true;
        } else if (arg == "--juce-android") {
            launchJuce = true;
            requestedPlatform = PlatformType::Android;
        }
    }

    Platform windowsPlatform({
        .type = PlatformType::Windows,
        .sampleRate = 48000.0,
        .blockSize = 256,
        .enableMidi = true,
        .enableTouch = false,
        .enableVibration = false});

    Platform androidPlatform({
        .type = PlatformType::Android,
        .sampleRate = 48000.0,
        .blockSize = 192,
        .enableMidi = true,
        .enableTouch = true,
        .enableVibration = true});

    std::cout << "Kraken Live Sequencer Demo\n";
    std::cout << "Desktop: " << windowsPlatform.describe() << '\n';
    std::cout << "Android: " << androidPlatform.describe() << '\n';

    Mixer mixer;
    Timeline timeline;
    ControllerManager controllers;
    std::shared_ptr<Clip> clipA;
    std::shared_ptr<Clip> clipB;

    AIAssistant assistant;
    assistant.setMode(AIMode::PassiveObserve);
    assistant.startSession("demo-session");

    kraken::runtime::AIDebugger debugger(assistant);
    kraken::runtime::ModuleOrchestrator orchestrator(debugger);

    auto mixerModule = std::make_unique<MixerBootstrapModule>(mixer);
    orchestrator.addModule(std::move(mixerModule));

    auto timelineModule = std::make_unique<TimelineBootstrapModule>(timeline, 132.0);
    const auto* timelineModulePtr = timelineModule.get();
    orchestrator.addModule(std::move(timelineModule));

    auto controllerModule = std::make_unique<ControllerBootstrapModule>(controllers, assistant);
    orchestrator.addModule(std::move(controllerModule));

    auto clipModule = std::make_unique<ClipSchedulingModule>(
        timeline, *timelineModulePtr, demoStemSource, clipA, clipB);
    orchestrator.addModule(std::move(clipModule));

    if (!orchestrator.startAll()) {
        std::cerr << "Failed to start Kraken runtime modules.\n";
        assistant.endSession();
        return 2;
    }

    if (launchJuce) {
#if KRAKEN_HAS_JUCE
        const auto platformConfig = requestedPlatform == PlatformType::Android ? androidPlatform.config() : windowsPlatform.config();
        kraken::platform::JuceLaunchOptions options{
            .platform = platformConfig,
            .mixer = mixer,
            .timeline = timeline,
            .controllers = controllers,
            .assistant = assistant};
        const int result = kraken::platform::launchJuceSession(options);
        orchestrator.shutdownAll();
        assistant.endSession();
        return result;
#else
        std::cerr << "This build of Kraken does not include JUCE support. Reconfigure with KRAKEN_ENABLE_JUCE=ON.\n";
        orchestrator.shutdownAll();
        assistant.endSession();
        return 2;
#endif
    }

    assistant.setMode(AIMode::CollaborativeRecommend);
    const auto recs = assistant.recommendTracks("groove", 3);
    std::cout << "Recommended follow-ups:" << '\n';
    for (const auto& rec : recs) {
        std::cout << " - " << rec.trackId << " from " << rec.season << " | " << rec.rationale << '\n';
    }

    assistant.setMode(AIMode::AutonomousPerformer);

    double currentBeat = 0.0;
    const double beatsPerStep = 4.0;

    for (int step = 0; step < 20; ++step) {
        std::cout << "\n=== Beat " << currentBeat << " ===" << '\n';
        timeline.forEachActiveClip(currentBeat, [&](const ScheduledClip& clip) {
            if (!clip.clip->isPrepared()) {
                std::cout << "Preparing clip: " << clip.clip->name() << '\n';
                std::string prepareError;
                if (!clip.clip->prepare(&prepareError)) {
                    std::cout << "Failed to prepare clip: " << clip.clip->name();
                    if (!prepareError.empty()) {
                        std::cout << " (" << prepareError << ")";
                    }
                    std::cout << '\n';
                    return;
                }
            }
            std::cout << "Active clip: " << clip.clip->name() << " | length " << clip.clip->lengthSeconds() << "s" << '\n';
            for (const auto& effect : clip.effects.activeEffects(currentBeat)) {
                std::cout << "  FX: " << effect.name << " [" << effect.activationStartBeat << "-" << effect.activationEndBeat << "]" << '\n';
            }
        });

        assistant.update(currentBeat / (timeline.tempo() / 60.0));
        for (const auto& suggestion : assistant.liveSuggestions()) {
            std::cout << "Suggestion: " << suggestion.description << " (confidence " << suggestion.confidence << ")" << '\n';
        }

        std::this_thread::sleep_for(50ms);
        currentBeat += beatsPerStep;
    }

    orchestrator.shutdownAll();
    assistant.endSession();

    std::cout << "Session complete.\n";
    return 0;
}
