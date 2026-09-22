#pragma once

#include "RHI/api/RHITypes.hpp"

#include <cstdint>
#include <limits>
#include <string>

// RENDERING.md section 5.1/5.2 — the graph's declaration-side vocabulary. Every type here is
// plain data: no RHI device, no GPU resource, nothing that needs a Vulkan instance to exist.
// A GraphBuilder (GraphBuilder.hpp) is what turns these into a GraphDescription.
namespace Renderer
{
    using RGResourceId = uint32_t;
    using RGVersion     = uint32_t;

    inline constexpr RGResourceId INVALID_RG_RESOURCE_ID = std::numeric_limits<RGResourceId>::max();

    // A versioned handle to a graph texture. Every write verb (ColorTarget, DepthTarget,
    // StorageWrite) returns a new RGTexture whose Version is one higher than the handle it was
    // given — dependencies between passes are recovered by comparing these, never stated
    // directly (RENDERING.md section 5.1).
    struct RGTexture
    {
        RGResourceId Id      = INVALID_RG_RESOURCE_ID;
        RGVersion    Version = 0;

        bool operator==(const RGTexture& other) const
        {
            return Id == other.Id && Version == other.Version;
        }
        bool operator!=(const RGTexture& other) const { return !(*this == other); }
    };

    // Same versioning scheme as RGTexture, for buffers.
    struct RGBuffer
    {
        RGResourceId Id      = INVALID_RG_RESOURCE_ID;
        RGVersion    Version = 0;

        bool operator==(const RGBuffer& other) const
        {
            return Id == other.Id && Version == other.Version;
        }
        bool operator!=(const RGBuffer& other) const { return !(*this == other); }
    };

    // RENDERING.md section 4 — shared by graph resources and named render targets.
    enum class RGSizePolicyKind
    {
        Absolute,
        RelativeToResult,
        RelativeToSwapchain,
    };

    struct RGSizePolicy
    {
        RGSizePolicyKind Kind   = RGSizePolicyKind::Absolute;
        uint32_t         Width  = 0;    // Absolute only
        uint32_t         Height = 0;    // Absolute only
        float            Scale  = 1.0f; // RelativeToResult / RelativeToSwapchain only

        static RGSizePolicy MakeAbsolute(uint32_t width, uint32_t height)
        {
            RGSizePolicy policy;
            policy.Kind   = RGSizePolicyKind::Absolute;
            policy.Width  = width;
            policy.Height = height;
            return policy;
        }

        static RGSizePolicy MakeRelativeToResult(float scale = 1.0f)
        {
            RGSizePolicy policy;
            policy.Kind  = RGSizePolicyKind::RelativeToResult;
            policy.Scale = scale;
            return policy;
        }

        static RGSizePolicy MakeRelativeToSwapchain(float scale = 1.0f)
        {
            RGSizePolicy policy;
            policy.Kind  = RGSizePolicyKind::RelativeToSwapchain;
            policy.Scale = scale;
            return policy;
        }

        // Device-free by design (RENDERING.md section 5.3: "compilation is device-free").
        // referenceWidth/Height is the view's result size for RelativeToResult, or the
        // swapchain size for RelativeToSwapchain; ignored for Absolute.
        void Resolve(uint32_t referenceWidth, uint32_t referenceHeight,
                     uint32_t& outWidth, uint32_t& outHeight) const
        {
            switch (Kind)
            {
                case RGSizePolicyKind::Absolute:
                    outWidth  = Width;
                    outHeight = Height;
                    break;
                case RGSizePolicyKind::RelativeToResult:
                case RGSizePolicyKind::RelativeToSwapchain:
                    outWidth  = static_cast<uint32_t>(static_cast<float>(referenceWidth)  * Scale);
                    outHeight = static_cast<uint32_t>(static_cast<float>(referenceHeight) * Scale);
                    break;
            }
        }
    };

    struct RGTextureDesc
    {
        std::string         Name; // diagnostics only
        RHI::Format          Format = RHI::Format::Undefined;
        RGSizePolicy          Size;
        RHI::TextureUsage      Usage = RHI::TextureUsage::None;
    };

    struct RGBufferDesc
    {
        std::string      Name; // diagnostics only
        size_t            Size  = 0;
        RHI::BufferUsage   Usage = RHI::BufferUsage::None;
    };

    // One entry in the graph's ordered output list (RENDERING.md section 5.2). A view binds
    // its targets by slot index, never by Name — Name survives for diagnostics only.
    struct RGOutputSlot
    {
        std::string    Name;
        RHI::Format     Format = RHI::Format::Undefined;
        RGSizePolicy     Size;
    };

    // What a pass is doing with a resource it reads or writes — recorded per RGPassBuilder verb
    // so a later compiler (C2) can derive the RHI::ResourceState each usage implies without
    // re-deriving it from the verb name.
    enum class RGResourceUsage
    {
        SampledTexture,
        ColorTarget,
        DepthTarget,
        DepthReadOnly,
        StorageReadWrite,
        ReadBuffer,
        WriteBuffer,
    };
}
