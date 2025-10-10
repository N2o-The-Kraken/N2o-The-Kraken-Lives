#pragma once

#include "platform/Platform.h"

#ifndef KRAKEN_HAS_JUCE
#define KRAKEN_HAS_JUCE 0
#endif

namespace kraken::audio {
class Mixer;
class Timeline;
} // namespace kraken::audio

namespace kraken::controllers {
class ControllerManager;
} // namespace kraken::controllers

namespace kraken::ai {
class AIAssistant;
} // namespace kraken::ai

namespace kraken::platform {

struct JuceLaunchOptions {
    PlatformConfig platform;
    audio::Mixer& mixer;
    audio::Timeline& timeline;
    controllers::ControllerManager& controllers;
    ai::AIAssistant& assistant;
};

int launchJuceSession(const JuceLaunchOptions& options);

} // namespace kraken::platform
