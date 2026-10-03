#pragma once

#include <string_view>

namespace Editor
{
    // Whether text contains pattern, ignoring ASCII case; an empty pattern matches everything.
    // Allocates nothing, so a panel can filter every frame.
    [[nodiscard]] bool ContainsIgnoringCase(std::string_view text, std::string_view pattern);
}
