#pragma once

#include "HedgehogRenderer/Graph/RGTypes.hpp"

#include "RHI/api/RHITypes.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace RHI
{
    class IRHIDevice;
    class IRHISwapchain;
    class IRHITexture;
}

namespace Renderer
{
    struct TargetExtent
    {
        uint32_t Width  = 0;
        uint32_t Height = 0;

        bool IsZeroArea() const { return Width == 0 || Height == 0; }
        bool operator==(const TargetExtent&) const = default;
    };

    // A named texture target (RENDERING.md section 4). Size is Absolute (e.g. an editor panel,
    // resized through RequestResize) or RelativeToSwapchain. RelativeToResult is rejected: a target
    // is itself a view's result, so there is nothing for it to be relative to.
    struct RenderTargetDesc
    {
        std::string  Name;
        RHI::Format  Format = RHI::Format::Undefined;
        RGSizePolicy Size;
    };

    struct RenderTargetResult
    {
        bool        Success = false;
        std::string Message; // names the target; empty on success
    };

    enum class RenderTargetStatus
    {
        Ok,
        Unknown,  // no target has this name
        ZeroArea, // exists, but is 0 in width or height: not allocated, skip the view
    };

    struct ResolvedRenderTarget
    {
        RenderTargetStatus Status  = RenderTargetStatus::Unknown;
        RHI::IRHITexture*  Texture = nullptr; // non-null only when Status == Ok
        RHI::Format        Format  = RHI::Format::Undefined;
        TargetExtent       Extent;
        std::string        Message;           // names the target when Status != Ok
    };

    // Owns every named render target (RENDERING.md section 4): "main", the swapchain's current
    // image, and declared texture targets.
    //
    // Resizes are always deferred to the end of the frame, behind the frame fence, because the
    // editor resizes constantly as panels are dragged and resizing a texture the GPU is still
    // reading is a use-after-free. RequestResize and NotifySwapchainResized only queue a change;
    // EndFrame applies it, so every Resolve within a frame sees the same texture and extent. A
    // replaced texture is not destroyed at once: it is retired, tagged with the last frame that
    // could have used it, and released by the first BeginFrame whose completed-frame number shows
    // that frame's fence has signaled.
    //
    // Frame protocol: BeginFrame, any number of Resolve calls, EndFrame. Declare only between
    // frames. Assertions enforce both.
    //
    // Nothing calls this yet (views arrive in HE-78, the frame loop in HE-83).
    class RenderTargetRegistry
    {
    public:
        static constexpr std::string_view MAIN_TARGET = "main";

        RenderTargetRegistry(RHI::IRHIDevice& device, RHI::IRHISwapchain& swapchain);
        ~RenderTargetRegistry();

        RenderTargetRegistry(const RenderTargetRegistry&)            = delete;
        RenderTargetRegistry& operator=(const RenderTargetRegistry&) = delete;
        RenderTargetRegistry(RenderTargetRegistry&&)                 = delete;
        RenderTargetRegistry& operator=(RenderTargetRegistry&&)      = delete;

        // Creates the texture now, unless its extent is zero. Between frames only.
        [[nodiscard]] RenderTargetResult Declare(const RenderTargetDesc& desc);

        // Queues a new size for an Absolute target, applied by EndFrame. 0x0 is allowed: the
        // target becomes zero-area (e.g. a collapsed editor panel) and its texture is retired.
        [[nodiscard]] RenderTargetResult RequestResize(std::string_view name, uint32_t width, uint32_t height);

        // Queues a re-read of the swapchain's extent, applied by EndFrame to "main" and every
        // RelativeToSwapchain target. Call after the swapchain itself has been resized.
        void NotifySwapchainResized();

        // lastCompletedFrame: the newest frame number whose fence has signaled; retired textures
        // last used by that frame or earlier are released here.
        void BeginFrame(uint64_t frameNumber, uint64_t lastCompletedFrame, uint32_t swapchainImageIndex);

        // Applies every queued resize.
        void EndFrame();

        [[nodiscard]] ResolvedRenderTarget Resolve(std::string_view name) const;

        // A graph resource's extent under its size policy (RENDERING.md section 4). RelativeToResult
        // scales the extent of resultTarget, the target the view renders into; RelativeToSwapchain
        // scales the swapchain's. Status reports an unknown or zero-area result target.
        [[nodiscard]] ResolvedRenderTarget ResolveGraphResourceSize(const RGSizePolicy& size,
                                                                    std::string_view resultTarget) const;

        size_t GetRetiredTextureCount() const { return m_Retired.size(); }

    private:
        struct Target
        {
            RenderTargetDesc                  Desc;
            TargetExtent                      Extent;
            std::unique_ptr<RHI::IRHITexture> Texture; // null while zero-area
        };

        struct RetiredTexture
        {
            std::unique_ptr<RHI::IRHITexture> Texture;
            uint64_t                          LastUsedFrame = 0;
        };

        TargetExtent ComputeExtent(const RGSizePolicy& size) const;
        void         Reallocate(Target& target, const TargetExtent& extent);

        RHI::IRHIDevice&    m_Device;
        RHI::IRHISwapchain& m_Swapchain;

        std::unordered_map<std::string, Target>        m_Targets;
        std::unordered_map<std::string, TargetExtent>  m_PendingResizes;
        std::vector<RetiredTexture>                    m_Retired;

        TargetExtent m_SwapchainExtent;
        bool         m_SwapchainResizePending = false;

        bool     m_InFrame             = false;
        uint64_t m_FrameNumber         = 0;
        uint32_t m_SwapchainImageIndex = 0;
    };
}
