#include "TextSearch.hpp"

#include <algorithm>
#include <cctype>

namespace Editor
{
    bool ContainsIgnoringCase(std::string_view text, std::string_view pattern)
    {
        const auto equal = [](char a, char b)
        {
            return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
        };
        return pattern.empty() || !std::ranges::search(text, pattern, equal).empty();
    }
}
