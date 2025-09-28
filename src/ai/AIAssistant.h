#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace kraken::ai {

enum class AIMode {
    PassiveObserve,
    CollaborativeRecommend,
    AutonomousPerformer
};

struct SessionEvent {
    std::string trackId;
    std::string action;
    double timestampSeconds{};
};

struct Recommendation {
    std::string trackId;
    std::string season;
    std::string rationale;
};

struct AssistSuggestion {
    std::string description;
    double confidence{0.0};
};

class AIAssistant {
public:
    using Clock = std::chrono::steady_clock;

    explicit AIAssistant();

    void setMode(AIMode mode);
    [[nodiscard]] AIMode mode() const noexcept { return m_mode; }

    void startSession(const std::string& sessionId);
    void endSession();
    bool isSessionActive() const noexcept { return m_sessionActive; }

    void recordEvent(const SessionEvent& event);

    /// Called regularly from the audio thread to keep the assistant in sync with the timeline.
    void update(double timelinePositionSeconds);

    [[nodiscard]] std::vector<Recommendation> recommendTracks(
        std::optional<std::string> vibeTag,
        std::size_t count) const;

    [[nodiscard]] std::vector<AssistSuggestion> liveSuggestions() const;

    void markUserFeedback(const std::string& suggestionDescription, bool accepted);

    [[nodiscard]] bool isTrainingActive() const noexcept { return m_trainingActive; }

private:
    struct TrackProfile {
        std::string trackId;
        std::string season;
        std::vector<std::string> tags;
        double popularityBias{1.0};
    };

    struct SuggestionState {
        AssistSuggestion suggestion;
        Clock::time_point lastUpdated{};
        std::uint32_t acceptanceCount{};
        std::uint32_t rejectionCount{};
    };

    AIMode m_mode{AIMode::PassiveObserve};
    bool m_sessionActive{false};
    bool m_trainingActive{true};
    std::string m_sessionId;
    std::vector<SessionEvent> m_recentEvents;
    std::unordered_map<std::string, TrackProfile> m_library;
    mutable std::vector<SuggestionState> m_suggestions;

    void bootstrapLibrary();
    void refreshSuggestions(double timelinePositionSeconds) const;
};

} // namespace kraken::ai
