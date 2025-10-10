#pragma once

#include <optional>
#include <string>
#include <vector>

#include "audio/Clip.h"
#include "audio/ClipSource.h"

namespace kraken::audio {

class ClipLoader {
public:
    struct Result {
        std::optional<ClipBuffer> buffer;
        std::vector<std::string> warnings;
        std::string error;

        explicit operator bool() const noexcept { return buffer.has_value(); }
    };

    [[nodiscard]] Result load(const ClipSource& source, double expectedLengthSeconds) const;
};

} // namespace kraken::audio
