#include "HedgehogRenderer/Targets/RenderTargetRegistry.hpp"

#include "HedgehogRenderer/Graph/GraphInstantiator.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHISwapchain.hpp"
#include "RHI/api/IRHITexture.hpp"

#include <algorithm>
#include <cassert>

namespace Renderer
{
    namespace
    {
        std::string Quoted(std::string_view name)
        {
            return "render target '" + std::string(name) + "'";
        }

        ResolvedRenderTarget Unknown(std::string_view name)
        {
            ResolvedRenderTarget resolved;
            resolved.Status  = RenderTargetStatus::Unknown;
            resolved.Message = Quoted(name) + " is not declared";
            return resolved;
        }
    }

    RenderTargetRegistry::RenderTargetRegistry(RHI::IRHIDevice& device, RHI::IRHISwapchain& swapchain)
        : m_Device(device)
        , m_Swapchain(swapchain)
        , m_SwapchainExtent{ swapchain.GetWidth(), swapchain.GetHeight() }
    {
    }

    // The owner waits for the device to go idle before destroying the registry, as it does for
    // every other GPU resource; retired textures are released here with the rest.
    RenderTargetRegistry::~RenderTargetRegistry() = default;

    RenderTargetResult RenderTargetRegistry::Declare(const RenderTargetDesc& desc)
    {
        assert(!m_InFrame && "RenderTargetRegistry::Declare: targets are declared between frames.");

        if (desc.Name.empty() || desc.Name == MAIN_TARGET)
            return { false, Quoted(desc.Name) + ": the name is empty or reserved for the swapchain" };
        if (m_Targets.contains(desc.Name))
            return { false, Quoted(desc.Name) + " is already declared" };
        if (desc.Format == RHI::Format::Undefined)
            return { false, Quoted(desc.Name) + " has no format" };
        if (desc.Size.Kind == RGSizePolicyKind::RelativeToResult)
        {
            return { false, Quoted(desc.Name) + " cannot use RelativeToResult: a target is itself a view's "
                            "result; use Absolute or RelativeToSwapchain" };
        }

        Target& target = m_Targets[desc.Name];
        target.Desc = desc;
        Reallocate(target, ComputeExtent(desc.Size));
        return { true, {} };
    }

    RenderTargetResult RenderTargetRegistry::RequestResize(std::string_view name, uint32_t width, uint32_t height)
    {
        const auto it = m_Targets.find(std::string(name));
        if (it == m_Targets.end())
            return { false, Unknown(name).Message };
        if (it->second.Desc.Size.Kind != RGSizePolicyKind::Absolute)
            return { false, Quoted(name) + " follows the swapchain; only Absolute targets are resized directly" };

        m_PendingResizes[it->first] = { width, height };
        return { true, {} };
    }

    void RenderTargetRegistry::NotifySwapchainResized()
    {
        m_SwapchainResizePending = true;
    }

    void RenderTargetRegistry::BeginFrame(uint64_t frameNumber, uint64_t lastCompletedFrame,
                                          uint32_t swapchainImageIndex)
    {
        assert(!m_InFrame && "RenderTargetRegistry::BeginFrame: the previous frame never ended.");

        // Released only once the GPU is done with the last frame that could have sampled them.
        std::erase_if(m_Retired, [&](const RetiredTexture& retired)
        {
            return retired.LastUsedFrame <= lastCompletedFrame;
        });

        m_InFrame             = true;
        m_FrameNumber         = frameNumber;
        m_SwapchainImageIndex = swapchainImageIndex;
    }

    void RenderTargetRegistry::EndFrame()
    {
        assert(m_InFrame && "RenderTargetRegistry::EndFrame: no frame is in progress.");

        if (m_SwapchainResizePending)
        {
            m_SwapchainExtent        = { m_Swapchain.GetWidth(), m_Swapchain.GetHeight() };
            m_SwapchainResizePending = false;
            for (auto& [name, target] : m_Targets)
            {
                if (target.Desc.Size.Kind == RGSizePolicyKind::RelativeToSwapchain)
                    Reallocate(target, ComputeExtent(target.Desc.Size));
            }
        }

        for (const auto& [name, extent] : m_PendingResizes)
        {
            Target& target   = m_Targets.at(name);
            target.Desc.Size = RGSizePolicy::MakeAbsolute(extent.Width, extent.Height);
            Reallocate(target, extent);
        }
        m_PendingResizes.clear();

        m_InFrame = false;
    }

    ResolvedRenderTarget RenderTargetRegistry::Resolve(std::string_view name) const
    {
        ResolvedRenderTarget resolved;
        if (name == MAIN_TARGET)
        {
            resolved.Format = m_Swapchain.GetFormat();
            resolved.Extent = m_SwapchainExtent;
            // A minimized window may have no images at all, so only touch one when there is area.
            if (!resolved.Extent.IsZeroArea())
                resolved.Texture = &m_Swapchain.GetTexture(m_SwapchainImageIndex);
        }
        else
        {
            const auto it = m_Targets.find(std::string(name));
            if (it == m_Targets.end())
                return Unknown(name);
            resolved.Format  = it->second.Desc.Format;
            resolved.Extent  = it->second.Extent;
            resolved.Texture = it->second.Texture.get();
        }

        if (resolved.Extent.IsZeroArea())
        {
            resolved.Status  = RenderTargetStatus::ZeroArea;
            resolved.Texture = nullptr;
            resolved.Message = Quoted(name) + " has zero area (" + std::to_string(resolved.Extent.Width) + "x"
                             + std::to_string(resolved.Extent.Height) + ")";
            return resolved;
        }
        resolved.Status = RenderTargetStatus::Ok;
        return resolved;
    }

    ResolvedRenderTarget RenderTargetRegistry::ResolveGraphResourceSize(const RGSizePolicy& size,
                                                                        std::string_view resultTarget) const
    {
        const ResolvedRenderTarget result = Resolve(resultTarget);
        if (result.Status != RenderTargetStatus::Ok)
            return result;

        ResolvedRenderTarget resolved;
        resolved.Status = RenderTargetStatus::Ok;
        if (size.Kind == RGSizePolicyKind::RelativeToResult)
            size.Resolve(result.Extent.Width, result.Extent.Height, resolved.Extent.Width, resolved.Extent.Height);
        else
            resolved.Extent = ComputeExtent(size);

        if (resolved.Extent.IsZeroArea())
        {
            resolved.Status  = RenderTargetStatus::ZeroArea;
            resolved.Message = "a graph resource sized against " + Quoted(resultTarget) + " rounds to zero area";
        }
        return resolved;
    }

    TargetExtent RenderTargetRegistry::ComputeExtent(const RGSizePolicy& size) const
    {
        TargetExtent extent;
        size.Resolve(m_SwapchainExtent.Width, m_SwapchainExtent.Height, extent.Width, extent.Height);
        return extent;
    }

    void RenderTargetRegistry::Reallocate(Target& target, const TargetExtent& extent)
    {
        if (extent == target.Extent && (target.Texture || extent.IsZeroArea()))
            return;

        if (target.Texture)
            m_Retired.push_back({ std::move(target.Texture), m_FrameNumber });

        target.Extent = extent;
        if (extent.IsZeroArea())
            return; // reported by Resolve, never allocated

        RHI::TextureDesc desc;
        desc.Width  = extent.Width;
        desc.Height = extent.Height;
        desc.Format = target.Desc.Format;
        desc.Usage  = DefaultTextureUsage(target.Desc.Format);
        target.Texture = m_Device.CreateTexture(desc);
    }
}
