#include "audio/EffectChain.h"

#include <algorithm>
#include <ranges>

namespace kraken::audio {

EffectChain::EffectId EffectChain::addEffect(EffectNode node) {
    m_effects.push_back(std::move(node));
    return m_effects.size() - 1;
}

void EffectChain::linkEffects(EffectId source, const std::string& macroName) {
    if (source >= m_effects.size()) {
        return;
    }
    auto& node = m_effects[source];
    if (std::ranges::find(node.linkedMacros, macroName) == node.linkedMacros.end()) {
        node.linkedMacros.push_back(macroName);
    }
}

void EffectChain::schedule(EffectId effect, double startBeat, double endBeat) {
    if (effect >= m_effects.size()) {
        return;
    }
    auto& node = m_effects[effect];
    node.activationStartBeat = startBeat;
    node.activationEndBeat = endBeat;
}

std::vector<EffectNode> EffectChain::activeEffects(double beat) const {
    std::vector<EffectNode> active;
    for (const auto& node : m_effects) {
        if (beat >= node.activationStartBeat && beat <= node.activationEndBeat) {
            active.push_back(node);
        }
    }
    return active;
}

} // namespace kraken::audio
