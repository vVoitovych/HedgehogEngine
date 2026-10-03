#pragma once

#include <cstdint>

namespace HA
{
    inline constexpr uint32_t INVALID_AUDIO_INDEX = UINT32_MAX;

    // A clip loaded into an AudioEngine: decoded once, cached by path and shared by every sound
    // that plays it. The default is invalid, as is what a failed load returns.
    struct AudioClipId
    {
        uint32_t Index = INVALID_AUDIO_INDEX;

        [[nodiscard]] bool IsValid() const { return Index != INVALID_AUDIO_INDEX; }
        bool operator==(const AudioClipId&) const = default;
    };

    // One playing sound: a slot and the generation it had when the sound started. A handle whose
    // sound was stopped or has finished is stale, and every call taking it does nothing, even once
    // its slot plays another sound. The default is invalid, as is what a failed Play returns.
    struct SoundHandle
    {
        uint32_t Index      = INVALID_AUDIO_INDEX;
        uint32_t Generation = 0;

        [[nodiscard]] bool IsValid() const { return Index != INVALID_AUDIO_INDEX; }
        bool operator==(const SoundHandle&) const = default;
    };
}
