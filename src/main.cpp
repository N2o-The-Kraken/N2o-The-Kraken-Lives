#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <thread>

#include "ai/AIAssistant.h"
#include "audio/Clip.h"
#include "audio/EffectChain.h"
#include "audio/Mixer.h"
#include "audio/Timeline.h"
#include "controllers/ControllerManager.h"
#include "platform/Platform.h"

using namespace std::chrono_literals;

namespace {
std::filesystem::path demoStemZip(const std::string& trackId) {
    return std::filesystem::path{"stems"} / (trackId + ".zip");
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
}

int main() {
    using kraken::ai::AIAssistant;
    using kraken::ai::AIMode;
    using kraken::audio::Clip;
    using kraken::audio::EffectChain;
    using kraken::audio::Mixer;
    using kraken::audio::ScheduledClip;
    using kraken::audio::Timeline;
    using kraken::controllers::AkaiAPC64;
    using kraken::controllers::ControllerManager;
    using kraken::controllers::ControlMessage;
    using kraken::controllers::NativeInstrumentsX1;
    using kraken::controllers::NumarkPartyMix;
    using kraken::platform::Platform;
    using kraken::platform::PlatformConfig;
    using kraken::platform::PlatformType;

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
    auto deckA = mixer.createChannel("Deck A");
    auto deckB = mixer.createChannel("Deck B");
    auto deckC = mixer.createChannel("Deck C");

    Mixer::ChannelId fxBus = mixer.createChannel("Macro FX Bus");
    Mixer::ChannelId ambient = mixer.createChannel("Ambient Pad");

    mixer.updateChannel(fxBus, {.volume = 0.65, .pan = 0.0, .sendLevels = {{"reverb", 0.8}}});
    mixer.updateChannel(ambient, {.volume = 0.55, .pan = -0.25, .sendLevels = {{"delay", 0.4}}});

    Timeline timeline;
    timeline.setTempo(132.0);
    const auto deckATrack = timeline.createTrack("Deck A");
    const auto deckBTrack = timeline.createTrack("Deck B");

    AIAssistant assistant;
    assistant.setMode(AIMode::PassiveObserve);
    assistant.startSession("demo-session");

    // Set up controllers
    ControllerManager controllers;
    controllers.onMessage([&](kraken::controllers::ControllerType type, const ControlMessage& message) {
        std::cout << "Controller message from " << controllerName(type)
                  << ": " << message.control << " -> " << message.value << '\n';
        assistant.recordEvent({.trackId = message.control, .action = "tweak", .timestampSeconds = message.value});
    });

    controllers.addController(std::make_unique<AkaiAPC64>());
    controllers.addController(std::make_unique<NumarkPartyMix>());
    controllers.addController(std::make_unique<NativeInstrumentsX1>());

    // Schedule clips without loading all stems simultaneously.
    auto clipA = std::make_shared<Clip>("Season1 Intro Groove", demoStemZip("S1_HGWSR_001"), 64.0);
    ScheduledClip scA{
        .clip = clipA,
        .startBeat = 0.0,
        .durationBeats = 64.0,
    };
    auto fxChainA = scA.effects.addEffect({
        .name = "Low-pass wash",
        .type = kraken::audio::EffectType::Filter,
        .parameters = {.values = {{"cutoff", 2000.0}, {"resonance", 0.5}}},
        .activationStartBeat = 60.0,
        .activationEndBeat = 64.0});
    scA.effects.linkEffects(fxChainA, "macro_downbeat_transition");
    timeline.scheduleClip(deckATrack, std::move(scA));

    auto clipB = std::make_shared<Clip>("Season4 Perc Loop", demoStemZip("S4_HGWSR_042"), 32.0);
    ScheduledClip scB{
        .clip = clipB,
        .startBeat = 32.0,
        .durationBeats = 32.0,
    };
    auto fxChainB = scB.effects.addEffect({
        .name = "Granular riser",
        .type = kraken::audio::EffectType::Granular,
        .parameters = {.values = {{"density", 0.6}, {"spray", 0.2}}},
        .activationStartBeat = 56.0,
        .activationEndBeat = 64.0});
    scB.effects.linkEffects(fxChainB, "macro_downbeat_transition");
    timeline.scheduleClip(deckBTrack, std::move(scB));

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
                clip.clip->prepare();
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

    assistant.endSession();

    clipA->release();
    clipB->release();

    std::cout << "Session complete.\n";
    return 0;
}
