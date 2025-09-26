#include "ai/AIAssistant.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <random>
#include <sstream>
#include <ranges>

namespace kraken::ai {

namespace {
constexpr std::array<const char*, 6> kVibes{
    "groove",
    "percussive",
    "melodic",
    "ambient",
    "break",
    "fx"
};
}

auto makeTrackId(int season, int index) -> std::string {
    std::ostringstream oss;
    oss << "S" << season << "_HGWSR_";
    oss << std::setw(3) << std::setfill('0') << index;
    return oss.str();
}

auto makeSeasonName(int season) -> std::string {
    std::ostringstream oss;
    oss << "Hard Groove Wav Scroll Records Season " << season;
    return oss.str();
}

AIAssistant::AIAssistant() {
    bootstrapLibrary();
}

void AIAssistant::setMode(AIMode mode) {
    m_mode = mode;
    m_trainingActive = mode != AIMode::PassiveObserve ? true : m_trainingActive;
}

void AIAssistant::startSession(const std::string& sessionId) {
    m_sessionId = sessionId;
    m_sessionActive = true;
    m_trainingActive = true;
    m_recentEvents.clear();
}

void AIAssistant::endSession() {
    m_sessionActive = false;
    m_recentEvents.clear();
}

void AIAssistant::recordEvent(const SessionEvent& event) {
    if (!m_sessionActive) {
        return;
    }

    m_recentEvents.push_back(event);
    if (m_recentEvents.size() > 2048) {
        m_recentEvents.erase(m_recentEvents.begin(), m_recentEvents.begin() + 1024);
    }
}

void AIAssistant::update(double timelinePositionSeconds) {
    if (!m_sessionActive) {
        return;
    }

    refreshSuggestions(timelinePositionSeconds);
}

std::vector<Recommendation> AIAssistant::recommendTracks(
    std::optional<std::string> vibeTag,
    std::size_t count) const {
    std::vector<Recommendation> recommendations;
    recommendations.reserve(count);

    std::vector<const TrackProfile*> candidates;
    candidates.reserve(m_library.size());
    for (const auto& [id, profile] : m_library) {
        if (vibeTag) {
            if (std::ranges::find(profile.tags, *vibeTag) == profile.tags.end()) {
                continue;
            }
        }
        candidates.push_back(&profile);
    }

    if (candidates.empty()) {
        return recommendations;
    }

    std::mt19937 rng(static_cast<unsigned int>(m_recentEvents.size() * 7919 + count));

    std::ranges::shuffle(candidates, rng);
    const auto selectionCount = std::min(count, candidates.size());
    for (std::size_t i = 0; i < selectionCount; ++i) {
        const auto& profile = *candidates[i];
        Recommendation rec;
        rec.trackId = profile.trackId;
        rec.season = profile.season;
        std::ostringstream reason;
        reason << "Matches vibe: ";
        if (vibeTag) {
            reason << *vibeTag;
        } else if (!profile.tags.empty()) {
            reason << profile.tags.front();
        } else {
            reason << "unlabelled";
        }
        rec.rationale = reason.str();
        recommendations.push_back(std::move(rec));
    }

    return recommendations;
}

std::vector<AssistSuggestion> AIAssistant::liveSuggestions() const {
    std::vector<AssistSuggestion> suggestions;
    suggestions.reserve(m_suggestions.size());
    for (const auto& state : m_suggestions) {
        suggestions.push_back(state.suggestion);
    }
    return suggestions;
}

void AIAssistant::markUserFeedback(const std::string& description, bool accepted) {
    for (auto& state : m_suggestions) {
        if (state.suggestion.description == description) {
            if (accepted) {
                ++state.acceptanceCount;
            } else {
                ++state.rejectionCount;
            }
            return;
        }
    }
}

void AIAssistant::bootstrapLibrary() {
    using namespace std::string_literals;
    std::mt19937 rng{42};
    std::uniform_int_distribution<int> tagCountDist(2, 4);
    std::uniform_int_distribution<int> tagIndexDist(0, static_cast<int>(kVibes.size()) - 1);

    for (int season = 1; season <= 9; ++season) {
        for (int track = 1; track <= 100; ++track) {
            TrackProfile profile;
            profile.trackId = makeTrackId(season, track);
            profile.season = makeSeasonName(season);
            const int tagCount = tagCountDist(rng);
            for (int t = 0; t < tagCount; ++t) {
                profile.tags.emplace_back(kVibes[tagIndexDist(rng)]);
            }
            profile.popularityBias = 1.0 + (season * 0.05);
            m_library.emplace(profile.trackId, std::move(profile));
        }
    }
}

void AIAssistant::refreshSuggestions(double timelinePositionSeconds) const {
    const double beat = std::fmod(timelinePositionSeconds, 4.0);
    if (m_suggestions.empty()) {
        m_suggestions.resize(3);
    }

    const auto now = Clock::now();

    auto makeConfidence = [&](std::size_t index) {
        const double base = 0.6 + (0.1 * index);
        const double rhythmBias = 1.0 - std::abs(beat - index);
        return std::clamp(base * rhythmBias, 0.0, 1.0);
    };

    m_suggestions[0].suggestion = {
        .description = "Stage macro FX chain for next downbeat",
        .confidence = makeConfidence(0)
    };
    m_suggestions[0].lastUpdated = now;

    m_suggestions[1].suggestion = {
        .description = "Prepare collaborative stem rotation",
        .confidence = makeConfidence(1)
    };
    m_suggestions[1].lastUpdated = now;

    m_suggestions[2].suggestion = {
        .description = "Auto-blend decks C/D with percussive break",
        .confidence = makeConfidence(2)
    };
    m_suggestions[2].lastUpdated = now;
}

} // namespace kraken::ai
