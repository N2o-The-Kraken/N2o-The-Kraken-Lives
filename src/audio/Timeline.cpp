#include "audio/Timeline.h"

namespace kraken::audio {

Timeline::TrackId Timeline::createTrack(const std::string& name) {
    m_tracks.push_back(Track{.name = name});
    return m_tracks.size() - 1;
}

Track* Timeline::track(TrackId id) {
    if (id >= m_tracks.size()) {
        return nullptr;
    }
    return &m_tracks[id];
}

const Track* Timeline::track(TrackId id) const {
    if (id >= m_tracks.size()) {
        return nullptr;
    }
    return &m_tracks[id];
}

void Timeline::scheduleClip(TrackId trackId, ScheduledClip clip) {
    if (trackId >= m_tracks.size()) {
        return;
    }
    m_tracks[trackId].clips.push_back(std::move(clip));
}

void Timeline::forEachActiveClip(double currentBeat, const std::function<void(const ScheduledClip&)>& fn) const {
    for (const auto& track : m_tracks) {
        for (const auto& clip : track.clips) {
            if (currentBeat >= clip.startBeat && currentBeat <= (clip.startBeat + clip.durationBeats)) {
                fn(clip);
            }
        }
    }
}

} // namespace kraken::audio
