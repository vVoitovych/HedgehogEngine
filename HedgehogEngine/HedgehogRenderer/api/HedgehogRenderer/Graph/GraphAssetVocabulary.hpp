#pragma once

#include "RGTypes.hpp"

#include "RHI/api/RHITypes.hpp"

#include <optional>
#include <span>
#include <string_view>

// RENDERING.md section 6 — the strings a graph asset may use for engine values, and the only
// place they are mapped. Every string maps to exactly one value, and resolution is strict: an
// unknown or malformed string returns std::nullopt. Nothing here guesses, trims case or
// substitutes a default.
namespace Renderer
{
    struct FormatVocabularyEntry
    {
        std::string_view Name;
        RHI::Format      Value = RHI::Format::Undefined;
    };

    // Every RHI::Format except Undefined, spelled exactly as its enumerator (e.g. "D32Float").
    std::span<const FormatVocabularyEntry> GetFormatVocabulary();

    [[nodiscard]] std::optional<RHI::Format> ResolveFormat(std::string_view name);

    // Size-policy syntax:
    //   Absolute(<width>, <height>)   width, height: integers >= 1
    //   RelativeToResult(<scale>)     scale: finite decimal > 0
    //   RelativeToSwapchain(<scale>)
    // Whitespace is allowed around the arguments only.
    [[nodiscard]] std::optional<RGSizePolicy> ResolveSizePolicy(std::string_view text);

    // "true" or "false", nothing else, for boolean pass parameters.
    [[nodiscard]] std::optional<bool> ResolveFlag(std::string_view text);
}
