#pragma once

#include "../IRenderNode.hpp"

#include <Volk/volk.h>

#include <cstdint>
#include <memory>

namespace HW
{
    class Window;
}

namespace RHI
{
    class IRHIDevice;
    class IRHIRenderPass;
    class IRHIFramebuffer;
}

namespace Renderer
{
    class ResourceManager;

    class GuiNode final : public IRenderNode
    {
    public:
        GuiNode(HW::Window& window,
                RHI::IRHIDevice& device,
                const ResourceManager& resourceManager);
        ~GuiNode() override;

        GuiNode(const GuiNode&)            = delete;
        GuiNode& operator=(const GuiNode&) = delete;

        void Render(RenderContext& ctx) override;
        void Cleanup(RHI::IRHIDevice& device) override;

        void  BeginFrame();
        void  DiscardFrame();
        void  RecreateSceneDescriptor(const ResourceManager& rm);
        void* GetSceneViewTextureId() const;

        static bool IsCursorPositionInGUI();

    private:
        void RebuildIfNeeded(RHI::IRHIDevice& device, const ResourceManager& rm);
        void UploadFonts();
        void CreateSceneViewDescSet(const ResourceManager& resourceManager);

        std::unique_ptr<RHI::IRHIRenderPass>  m_RenderPass;
        std::unique_ptr<RHI::IRHIFramebuffer> m_FrameBuffer;
        VkDescriptorPool                      m_ImGuiPool        = VK_NULL_HANDLE;
        VkSampler                             m_SceneSampler     = VK_NULL_HANDLE;
        VkDescriptorSet                       m_SceneViewDescSet = VK_NULL_HANDLE;

        uint32_t m_CachedFbWidth    = 0;
        uint32_t m_CachedFbHeight   = 0;
        uint32_t m_CachedSceneWidth  = 0;
        uint32_t m_CachedSceneHeight = 0;
    };

} // namespace Renderer
