#pragma once

#include <string>

namespace Renderer::RenderGraphVocabulary
{
    // Pure membership checks against the .rgq schema's (version 1) string vocabulary — used by
    // RenderGraphLoader::Parse to implement validation rules V5 and V7. Deliberately just
    // membership checks, not resolvers into RHI::Format/TextureUsage/SizeClass: this session's
    // Phase 5 design keeps resource creation pass-owned (see RenderGraphDesc.hpp), so nothing
    // needs to turn these strings into RHI types — only validate that they're well-formed.
    // See workflow/current-plan.md, ".rgq schema (version 1)" for the authoritative tables.

    // "depth_preferred", "depth32", "depth16", "rgba8", "rgba8_srgb", "bgra8", "rgba16",
    // "rgba16f", "r16f", "r32f".
    [[nodiscard]] bool IsKnownFormat(const std::string& name);

    // "view", "swapchain", "fixed".
    [[nodiscard]] bool IsKnownSizeClass(const std::string& name);

    // "sampled", "color_attachment", "depth_stencil", "transfer_src", "transfer_dst", "storage".
    [[nodiscard]] bool IsKnownUsageFlag(const std::string& name);

    // "colorWrite", "depthWrite", "shadowWrite", "depthRead", "shaderRead", "shadowRead",
    // "presentSource".
    [[nodiscard]] bool IsKnownSlotUsage(const std::string& name);
}
