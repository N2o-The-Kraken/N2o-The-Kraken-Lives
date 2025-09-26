#pragma once

#include <map>
#include <string>
#include <vector>

namespace kraken::audio {

enum class EffectType {
    Filter,
    Delay,
    Reverb,
    Distortion,
    Granular,
    Custom
};

struct EffectParameters {
    std::map<std::string, double> values;
};

struct EffectNode {
    std::string name;
    EffectType type{EffectType::Custom};
    EffectParameters parameters;
    double activationStartBeat{};
    double activationEndBeat{};
    std::vector<std::string> linkedMacros;
};

class EffectChain {
public:
    using EffectId = std::size_t;

    EffectId addEffect(EffectNode node);
    void linkEffects(EffectId source, const std::string& macroName);
    void schedule(EffectId effect, double startBeat, double endBeat);
    [[nodiscard]] std::vector<EffectNode> activeEffects(double beat) const;

    [[nodiscard]] const std::vector<EffectNode>& effects() const noexcept { return m_effects; }

private:
    std::vector<EffectNode> m_effects;
};

} // namespace kraken::audio
