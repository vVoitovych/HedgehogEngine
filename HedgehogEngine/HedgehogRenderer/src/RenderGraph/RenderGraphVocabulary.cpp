#include "RenderGraphVocabulary.hpp"

#include <unordered_set>

namespace Renderer::RenderGraphVocabulary
{
namespace
{
    const std::unordered_set<std::string> kFormats = {
        "depth_preferred", "depth32", "depth16",
        "rgba8", "rgba8_srgb", "bgra8", "rgba16", "rgba16f", "r16f", "r32f",
    };

    const std::unordered_set<std::string> kSizeClasses = { "view", "swapchain", "fixed" };

    const std::unordered_set<std::string> kUsageFlags = {
        "sampled", "color_attachment", "depth_stencil", "transfer_src", "transfer_dst", "storage",
    };

    const std::unordered_set<std::string> kSlotUsages = {
        "colorWrite", "depthWrite", "shadowWrite",
        "depthRead", "shaderRead", "shadowRead", "presentSource",
    };
}

    bool IsKnownFormat(const std::string& name)    { return kFormats.count(name) != 0; }
    bool IsKnownSizeClass(const std::string& name) { return kSizeClasses.count(name) != 0; }
    bool IsKnownUsageFlag(const std::string& name) { return kUsageFlags.count(name) != 0; }
    bool IsKnownSlotUsage(const std::string& name) { return kSlotUsages.count(name) != 0; }
}
