#pragma once

#include "NodeConfig.hpp"

#include <string>
#include <vector>

namespace Renderer
{
    inline constexpr const char* MAIN_RENDER_QUEUE_PATH =
        "/HedgehogEngine/HedgehogRenderer/Assets/RenderQueue/Main.rq";

    // Parses a .rq YAML file into an ordered list of NodeConfig structs.
    // Pure data; no GPU work, no node construction.
    class RenderQueueLoader
    {
    public:
        // assetRelativePath must begin with '/' and be relative to the repo root.
        static std::vector<NodeConfig> Load(const std::string& assetRelativePath);
    };

} // namespace Renderer
