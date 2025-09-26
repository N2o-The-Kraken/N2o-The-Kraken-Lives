#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "audio/Clip.h"
#include "audio/EffectChain.h"

namespace kraken::audio {

struct ScheduledClip {
    std::shared_ptr<Clip> clip;
    double startBeat{};
    double durationBeats{};
    EffectChain effects;
};

struct Track {
    std::string name;
    std::vector<ScheduledClip> clips;
};

class Timeline {
public:
    using TrackId = std::size_t;

    void setTempo(double bpm) { m_bpm = bpm; }
    [[nodiscard]] double tempo() const noexcept { return m_bpm; }

    TrackId createTrack(const std::string& name);
    [[nodiscard]] Track* track(TrackId id);
    [[nodiscard]] const Track* track(TrackId id) const;

    void scheduleClip(TrackId trackId, ScheduledClip clip);

    void forEachActiveClip(double currentBeat, const std::function<void(const ScheduledClip&)>& fn) const;

private:
    double m_bpm{128.0};
    std::vector<Track> m_tracks;
};

} // namespace kraken::audio
