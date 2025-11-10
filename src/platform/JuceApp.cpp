#include "platform/JuceApp.h"

#if KRAKEN_HAS_JUCE

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <mutex>
#include <sstream>
#include <vector>

#include "ai/AIAssistant.h"
#include "audio/Mixer.h"
#include "audio/Timeline.h"
#include "controllers/ControllerManager.h"

namespace kraken::platform {
namespace {

controllers::ControllerType detectControllerType(const juce::String& name) {
    const auto lowered = name.toLowerCase();
    if (lowered.contains("apc")) {
        return controllers::ControllerType::AkaiAPC64;
    }
    if (lowered.contains("numark")) {
        return controllers::ControllerType::NumarkPartyMix;
    }
    return controllers::ControllerType::NativeInstrumentsX1;
}

class KrakenAudioEngine : public juce::AudioIODeviceCallback, public juce::MidiInputCallback {
public:
    KrakenAudioEngine(PlatformConfig config,
                      audio::Mixer& mixer,
                      controllers::ControllerManager& controllers)
        : m_config(config), m_mixer(mixer), m_controllers(controllers) {}

    ~KrakenAudioEngine() override { shutdown(); }

    void initialise() {
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        setup.sampleRate = m_config.sampleRate;
        setup.bufferSize = static_cast<int>(m_config.blockSize);

        juce::String error = m_deviceManager.initialiseWithDefaultDevices(0, 2);
        if (error.isNotEmpty()) {
            juce::Logger::writeToLog("Audio device init failed: " + error);
        }

        if (auto* device = m_deviceManager.getCurrentAudioDevice()) {
            if (m_config.sampleRate > 0.0) {
                device->setCurrentSampleRate(m_config.sampleRate);
            }
            if (m_config.blockSize > 0) {
                device->setCurrentBufferSizeSamples(static_cast<int>(m_config.blockSize));
            }
        }

        m_deviceManager.addAudioCallback(this);
        if (m_config.enableMidi) {
            for (const auto& midi : juce::MidiInput::getAvailableDevices()) {
                m_deviceManager.setMidiInputDeviceEnabled(midi.identifier, true);
                m_deviceManager.addMidiInputDeviceCallback(midi.identifier, this);
                m_enabledMidiInputs.push_back(midi);
            }
        }

        m_channelSyncNeeded.store(true);
        m_elapsedSeconds.store(0.0);
    }

    void shutdown() {
        if (m_shutdown) {
            return;
        }
        m_shutdown = true;

        if (m_config.enableMidi) {
            for (const auto& midi : m_enabledMidiInputs) {
                m_deviceManager.removeMidiInputDeviceCallback(midi.identifier, this);
            }
        }
        m_deviceManager.removeAudioCallback(this);
        m_enabledMidiInputs.clear();
    }

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override {
        std::scoped_lock lock(m_channelMutex);
        m_sampleRate = device != nullptr ? device->getCurrentSampleRate() : m_config.sampleRate;
        ensureScratchBuffer(device != nullptr ? device->getCurrentBufferSizeSamples()
                                              : static_cast<int>(m_config.blockSize));
        m_channelSyncNeeded.store(true, std::memory_order_relaxed);
    }

    void audioDeviceStopped() override {
        std::scoped_lock lock(m_channelMutex);
        m_sampleRate = m_config.sampleRate;
    }

    void audioDeviceIOCallback(const float** /*inputChannelData*/,
                               int /*numInputChannels*/,
                               float** outputChannelData,
                               int numOutputChannels,
                               int numSamples) override {
        juce::ScopedNoDenormals noDenormals;
        std::scoped_lock lock(m_channelMutex);

        ensureScratchBuffer(numSamples);
        if (m_channelSyncNeeded.exchange(false)) {
            rebuildOscillatorsLocked();
        }

        std::fill(m_mixScratch.begin(), m_mixScratch.begin() + numSamples, 0.0f);

        m_mixer.forEachChannel([&](audio::Mixer::ChannelId id, const audio::ChannelSettings& settings) {
            if (id >= m_oscillators.size()) {
                return;
            }
            auto& osc = m_oscillators[id];
            const float gain = static_cast<float>(juce::jlimit(0.0, 1.25, settings.volume));
            for (int sample = 0; sample < numSamples; ++sample) {
                const float value = std::sin(osc.phase) * gain;
                m_mixScratch[sample] += value;
                osc.phase += osc.phaseDelta;
                if (osc.phase > juce::MathConstants<double>::twoPi) {
                    osc.phase -= juce::MathConstants<double>::twoPi;
                }
            }
        });

        const float scalar = m_outputGain.load(std::memory_order_relaxed);
        for (int channel = 0; channel < numOutputChannels; ++channel) {
            for (int sample = 0; sample < numSamples; ++sample) {
                outputChannelData[channel][sample] = juce::jlimit(-1.0f, 1.0f, m_mixScratch[sample] * scalar);
            }
        }

        if (m_sampleRate > 0.0) {
            const double elapsed = static_cast<double>(numSamples) / m_sampleRate;
            m_elapsedSeconds.fetch_add(elapsed, std::memory_order_relaxed);
        }
    }

    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override {
        if (!m_config.enableMidi) {
            return;
        }
        controllers::ControlMessage msg{
            .control = message.getDescription().toStdString(),
            .value = message.isController() ? message.getControllerValue() / 127.0
                                            : (message.isNoteOn() ? message.getFloatVelocity() : 0.0)};
        const auto type = detectControllerType(source != nullptr ? source->getName() : juce::String{});
        m_controllers.emitVirtualMessage(type, msg);
    }

    juce::AudioDeviceManager& deviceManager() noexcept { return m_deviceManager; }

    std::string describeCurrentDevice() const {
        if (auto* device = m_deviceManager.getCurrentAudioDevice()) {
            std::ostringstream oss;
            oss << device->getName().toStdString();
            oss << " @ " << device->getCurrentSampleRate() << " Hz";
            oss << " | buffer " << device->getCurrentBufferSizeSamples() << " samples";
            return oss.str();
        }
        return "No audio device";
    }

    std::vector<std::string> enabledMidiInputs() const {
        std::vector<std::string> inputs;
        inputs.reserve(m_enabledMidiInputs.size());
        for (const auto& midi : m_enabledMidiInputs) {
            inputs.push_back(midi.name.toStdString());
        }
        return inputs;
    }

    std::string describeMidiInputs() const {
        if (m_enabledMidiInputs.empty()) {
            return "No MIDI inputs enabled";
        }
        std::ostringstream oss;
        bool first = true;
        for (const auto& midi : m_enabledMidiInputs) {
            if (!first) {
                oss << ", ";
            }
            first = false;
            oss << midi.name.toStdString();
        }
        return oss.str();
    }

    void setOutputGain(float gain) { m_outputGain.store(gain, std::memory_order_relaxed); }

    double elapsedSeconds() const { return m_elapsedSeconds.load(std::memory_order_relaxed); }

    void requestChannelSync() { m_channelSyncNeeded.store(true, std::memory_order_relaxed); }

private:
    struct Oscillator {
        double phase{0.0};
        double phaseDelta{0.0};
    };

    void ensureScratchBuffer(int numSamples) {
        if (numSamples <= 0) {
            numSamples = static_cast<int>(m_config.blockSize);
        }
        if (static_cast<int>(m_mixScratch.size()) < numSamples) {
            m_mixScratch.resize(static_cast<std::size_t>(numSamples));
        }
    }

    void rebuildOscillatorsLocked() {
        const auto channelCount = m_mixer.channelNames().size();
        m_oscillators.resize(channelCount);
        const double base = 55.0;
        for (std::size_t i = 0; i < channelCount; ++i) {
            m_oscillators[i].phase = 0.0;
            const double freq = base + (static_cast<double>(i) * 20.0);
            const double rate = m_sampleRate > 0.0 ? m_sampleRate : m_config.sampleRate;
            const double period = juce::MathConstants<double>::twoPi * freq;
            m_oscillators[i].phaseDelta = rate > 0.0 ? period / rate : 0.0;
        }
    }

    PlatformConfig m_config;
    audio::Mixer& m_mixer;
    controllers::ControllerManager& m_controllers;

    juce::AudioDeviceManager m_deviceManager;
    std::vector<juce::MidiDeviceInfo> m_enabledMidiInputs;

    std::vector<float> m_mixScratch;
    std::vector<Oscillator> m_oscillators;

    std::atomic<float> m_outputGain{1.0f};
    std::atomic<double> m_elapsedSeconds{0.0};
    std::atomic<bool> m_channelSyncNeeded{false};

    double m_sampleRate{0.0};
    bool m_shutdown{false};

    std::mutex m_channelMutex;
};

class MixerListModel : public juce::ListBoxModel {
public:
    explicit MixerListModel(audio::Mixer& mixer) : m_mixer(mixer) {}

    int getNumRows() override {
        return static_cast<int>(m_mixer.channelNames().size());
    }

    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool rowIsSelected) override {
        auto names = m_mixer.channelNames();
        if (row < 0 || static_cast<std::size_t>(row) >= names.size()) {
            return;
        }
        const auto* settings = m_mixer.channel(static_cast<audio::Mixer::ChannelId>(row));
        juce::ignoreUnused(rowIsSelected);
        juce::String text = juce::String(names[row]) + " | vol "
            + juce::String(settings ? settings->volume : 0.0, 2)
            + " | pan " + juce::String(settings ? settings->pan : 0.0, 2);
        g.setColour(juce::Colours::lightgreen);
        g.drawText(text, 0, 0, width, height, juce::Justification::centredLeft, true);
    }

private:
    audio::Mixer& m_mixer;
};

class TimelineListModel : public juce::ListBoxModel {
public:
    explicit TimelineListModel(audio::Timeline& timeline) : m_timeline(timeline) {}

    int getNumRows() override {
        refreshRows();
        return static_cast<int>(m_rows.size());
    }

    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool /*rowIsSelected*/) override {
        if (row < 0 || static_cast<std::size_t>(row) >= m_rows.size()) {
            return;
        }
        g.setColour(juce::Colours::skyblue);
        g.drawText(m_rows[row], 0, 0, width, height, juce::Justification::centredLeft, true);
    }

    void refreshRows() {
        m_rows.clear();
        for (std::size_t trackId = 0;; ++trackId) {
            const auto* track = m_timeline.track(trackId);
            if (track == nullptr) {
                break;
            }
            for (const auto& clip : track->clips) {
                std::ostringstream oss;
                oss << track->name << " | "
                    << (clip.clip ? clip.clip->name() : "<unloaded>")
                    << " @ beat " << clip.startBeat << " -> "
                    << (clip.startBeat + clip.durationBeats);
                m_rows.emplace_back(oss.str());
            }
        }
    }

private:
    audio::Timeline& m_timeline;
    std::vector<std::string> m_rows;
};

class SuggestionListModel : public juce::ListBoxModel {
public:
    explicit SuggestionListModel(ai::AIAssistant& assistant) : m_assistant(assistant) {}

    int getNumRows() override {
        refresh();
        return static_cast<int>(m_rows.size());
    }

    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool /*rowIsSelected*/) override {
        if (row < 0 || static_cast<std::size_t>(row) >= m_rows.size()) {
            return;
        }
        g.setColour(juce::Colours::orange);
        g.drawText(m_rows[row], 0, 0, width, height, juce::Justification::centredLeft, true);
    }

    void refresh() {
        m_rows.clear();
        for (const auto& suggestion : m_assistant.liveSuggestions()) {
            std::ostringstream oss;
            oss << suggestion.description << " (" << juce::String(suggestion.confidence, 2).toStdString() << ")";
            m_rows.emplace_back(oss.str());
        }
    }

private:
    ai::AIAssistant& m_assistant;
    std::vector<std::string> m_rows;
};

class KrakenMainComponent : public juce::Component, private juce::Timer {
public:
    KrakenMainComponent(PlatformConfig config,
                        KrakenAudioEngine& engine,
                        audio::Mixer& mixer,
                        audio::Timeline& timeline,
                        ai::AIAssistant& assistant)
        : m_config(config),
          m_engine(engine),
          m_assistant(assistant),
          m_mixerModel(mixer),
          m_timelineModel(timeline),
          m_suggestionModel(assistant),
          m_deviceSelector(engine.deviceManager(), 0, 2, 0, 2, true, config.enableMidi, true, false) {
        addAndMakeVisible(m_deviceSelector);

        m_platformLabel.setText("Platform: " + juce::String(config.type == PlatformType::Windows ? "Windows" : "Android"),
                                juce::dontSendNotification);
        addAndMakeVisible(m_platformLabel);

        m_audioDeviceLabel.setText("Audio: " + juce::String(engine.describeCurrentDevice()), juce::dontSendNotification);
        addAndMakeVisible(m_audioDeviceLabel);

        m_midiLabel.setText("MIDI: " + juce::String(engine.describeMidiInputs()), juce::dontSendNotification);
        addAndMakeVisible(m_midiLabel);

        addAndMakeVisible(m_mixerList);
        m_mixerList.setModel(&m_mixerModel);

        addAndMakeVisible(m_timelineList);
        m_timelineList.setModel(&m_timelineModel);

        addAndMakeVisible(m_suggestionList);
        m_suggestionList.setModel(&m_suggestionModel);

        m_gainSlider.setRange(0.0, 2.0, 0.01);
        m_gainSlider.setValue(1.0);
        m_gainSlider.onValueChange = [this] { m_engine.setOutputGain(static_cast<float>(m_gainSlider.getValue())); };
        addAndMakeVisible(m_gainSlider);
        m_gainLabel.setText("Output Gain", juce::dontSendNotification);
        addAndMakeVisible(m_gainLabel);

        startTimerHz(30);
    }

    void resized() override {
        auto bounds = getLocalBounds().reduced(10);
        auto header = bounds.removeFromTop(60);
        m_platformLabel.setBounds(header.removeFromLeft(header.getWidth() / 3));
        m_audioDeviceLabel.setBounds(header);

        auto midiRow = bounds.removeFromTop(50);
        m_midiLabel.setBounds(midiRow.removeFromLeft(midiRow.getWidth() * 2 / 3));
        m_gainLabel.setBounds(midiRow.removeFromLeft(120));
        m_gainSlider.setBounds(midiRow);

        auto bottom = bounds;
        auto left = bottom.removeFromLeft(bottom.getWidth() * 2 / 5);
        m_deviceSelector.setBounds(left.removeFromTop(left.getHeight() / 2));
        m_mixerList.setBounds(left);

        auto centre = bottom.removeFromLeft(bottom.getWidth() / 2);
        m_timelineList.setBounds(centre);

        m_suggestionList.setBounds(bottom);
    }

private:
    void timerCallback() override {
        m_engine.requestChannelSync();
        m_audioDeviceLabel.setText("Audio: " + juce::String(m_engine.describeCurrentDevice()), juce::dontSendNotification);
        m_midiLabel.setText("MIDI: " + juce::String(m_engine.describeMidiInputs()), juce::dontSendNotification);
        m_assistant.update(m_engine.elapsedSeconds());
        m_mixerList.updateContent();
        m_timelineList.updateContent();
        m_suggestionList.updateContent();
        repaint();
    }

    PlatformConfig m_config;
    KrakenAudioEngine& m_engine;
    ai::AIAssistant& m_assistant;

    juce::Label m_platformLabel;
    juce::Label m_audioDeviceLabel;
    juce::Label m_midiLabel;

    MixerListModel m_mixerModel;
    juce::ListBox m_mixerList{ "Mixer", &m_mixerModel };

    TimelineListModel m_timelineModel;
    juce::ListBox m_timelineList{ "Timeline", &m_timelineModel };

    SuggestionListModel m_suggestionModel;
    juce::ListBox m_suggestionList{ "AI", &m_suggestionModel };

    juce::Slider m_gainSlider;
    juce::Label m_gainLabel;

    juce::AudioDeviceSelectorComponent m_deviceSelector;
};

class KrakenMainWindow : public juce::DocumentWindow {
public:
    KrakenMainWindow(PlatformConfig config,
                     KrakenAudioEngine& engine,
                     audio::Mixer& mixer,
                     audio::Timeline& timeline,
                     ai::AIAssistant& assistant)
        : DocumentWindow("Kraken Live", juce::Colours::black, DocumentWindow::allButtons) {
        setUsingNativeTitleBar(true);
        setResizable(true, true);
        setContentOwned(new KrakenMainComponent(config, engine, mixer, timeline, assistant), true);
        centreWithSize(1100, 720);
        setVisible(true);
    }

    void closeButtonPressed() override {
        juce::MessageManager::getInstance()->stopDispatchLoop();
    }
};

} // namespace

int launchJuceSession(const JuceLaunchOptions& options) {
    juce::ScopedJuceInitialiser_GUI init;
    KrakenAudioEngine engine(options.platform, options.mixer, options.controllers);
    engine.initialise();

    auto window = std::make_unique<KrakenMainWindow>(options.platform, engine, options.mixer, options.timeline, options.assistant);
    juce::MessageManager::getInstance()->runDispatchLoop();
    window.reset();
    engine.shutdown();
    return 0;
}

} // namespace kraken::platform

#else

namespace kraken::platform {

int launchJuceSession(const JuceLaunchOptions&) {
    return -1;
}

} // namespace kraken::platform

#endif
