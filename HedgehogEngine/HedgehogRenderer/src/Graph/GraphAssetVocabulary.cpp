#include "HedgehogRenderer/Graph/GraphAssetVocabulary.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <string>
#include <vector>

namespace Renderer
{
    namespace
    {
        constexpr std::array<FormatVocabularyEntry, 16> FORMAT_VOCABULARY =
        {{
            { "R8Unorm",            RHI::Format::R8Unorm },
            { "R8G8B8A8Unorm",      RHI::Format::R8G8B8A8Unorm },
            { "R8G8B8A8Srgb",       RHI::Format::R8G8B8A8Srgb },
            { "B8G8R8A8Unorm",      RHI::Format::B8G8R8A8Unorm },
            { "B8G8R8A8Srgb",       RHI::Format::B8G8R8A8Srgb },
            { "R16Float",           RHI::Format::R16Float },
            { "R16G16B16A16Unorm",  RHI::Format::R16G16B16A16Unorm },
            { "R16G16B16A16Float",  RHI::Format::R16G16B16A16Float },
            { "R32Float",           RHI::Format::R32Float },
            { "R32G32Float",        RHI::Format::R32G32Float },
            { "R32G32B32Float",     RHI::Format::R32G32B32Float },
            { "R32G32B32A32Float",  RHI::Format::R32G32B32A32Float },
            { "D16Unorm",           RHI::Format::D16Unorm },
            { "D32Float",           RHI::Format::D32Float },
            { "D24UnormS8Uint",     RHI::Format::D24UnormS8Uint },
            { "D32FloatS8Uint",     RHI::Format::D32FloatS8Uint },
        }};

        struct SizePolicyKindEntry
        {
            std::string_view Name;
            RGSizePolicyKind Value = RGSizePolicyKind::Absolute;
        };

        constexpr std::array<SizePolicyKindEntry, 3> SIZE_POLICY_KIND_VOCABULARY =
        {{
            { "Absolute",            RGSizePolicyKind::Absolute },
            { "RelativeToResult",    RGSizePolicyKind::RelativeToResult },
            { "RelativeToSwapchain", RGSizePolicyKind::RelativeToSwapchain },
        }};

        std::string_view Trim(std::string_view text)
        {
            constexpr std::string_view WHITESPACE = " \t";
            const size_t first = text.find_first_not_of(WHITESPACE);
            if (first == std::string_view::npos)
                return {};
            const size_t last = text.find_last_not_of(WHITESPACE);
            return text.substr(first, last - first + 1);
        }

        // Splits "a, b" into trimmed arguments. An empty argument (e.g. "1,,2") comes back empty
        // and fails the number parse that follows, so it is never silently skipped.
        std::vector<std::string_view> SplitArguments(std::string_view arguments)
        {
            std::vector<std::string_view> result;
            size_t start = 0;
            while (true)
            {
                const size_t comma = arguments.find(',', start);
                result.push_back(Trim(arguments.substr(start, comma - start)));
                if (comma == std::string_view::npos)
                    break;
                start = comma + 1;
            }
            return result;
        }

        // from_chars accepts neither a leading '+' nor whitespace, and the whole token must be
        // consumed, so "12px" or "1e" is rejected rather than read as a prefix.
        std::optional<uint32_t> ParsePositiveInteger(std::string_view text)
        {
            uint32_t value = 0;
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            if (error != std::errc{} || end != text.data() + text.size() || value == 0)
                return std::nullopt;
            return value;
        }

        std::optional<float> ParsePositiveScale(std::string_view text)
        {
            float value = 0.0f;
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value,
                                                      std::chars_format::fixed);
            if (error != std::errc{} || end != text.data() + text.size())
                return std::nullopt;
            if (!std::isfinite(value) || value <= 0.0f)
                return std::nullopt;
            return value;
        }

        std::optional<RGSizePolicyKind> ResolveSizePolicyKind(std::string_view name)
        {
            for (const auto& entry : SIZE_POLICY_KIND_VOCABULARY)
            {
                if (entry.Name == name)
                    return entry.Value;
            }
            return std::nullopt;
        }
    }

    std::span<const FormatVocabularyEntry> GetFormatVocabulary()
    {
        return FORMAT_VOCABULARY;
    }

    std::optional<RHI::Format> ResolveFormat(std::string_view name)
    {
        for (const auto& entry : FORMAT_VOCABULARY)
        {
            if (entry.Name == name)
                return entry.Value;
        }
        return std::nullopt;
    }

    std::optional<RGSizePolicy> ResolveSizePolicy(std::string_view text)
    {
        const size_t open = text.find('(');
        if (open == std::string_view::npos || text.empty() || text.back() != ')')
            return std::nullopt;

        const std::optional<RGSizePolicyKind> kind = ResolveSizePolicyKind(text.substr(0, open));
        if (!kind)
            return std::nullopt;

        const std::vector<std::string_view> arguments =
            SplitArguments(text.substr(open + 1, text.size() - open - 2));

        if (*kind == RGSizePolicyKind::Absolute)
        {
            if (arguments.size() != 2)
                return std::nullopt;
            const std::optional<uint32_t> width  = ParsePositiveInteger(arguments[0]);
            const std::optional<uint32_t> height = ParsePositiveInteger(arguments[1]);
            if (!width || !height)
                return std::nullopt;
            return RGSizePolicy::MakeAbsolute(*width, *height);
        }

        if (arguments.size() != 1)
            return std::nullopt;
        const std::optional<float> scale = ParsePositiveScale(arguments[0]);
        if (!scale)
            return std::nullopt;
        return *kind == RGSizePolicyKind::RelativeToResult
            ? RGSizePolicy::MakeRelativeToResult(*scale)
            : RGSizePolicy::MakeRelativeToSwapchain(*scale);
    }

    std::optional<bool> ResolveFlag(std::string_view text)
    {
        if (text == "true")
            return true;
        if (text == "false")
            return false;
        return std::nullopt;
    }
}
