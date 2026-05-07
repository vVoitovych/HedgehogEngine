#pragma once

#include "HedgehogRenderer/IRenderNode.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace HW             { class Window;           }
namespace RHI            { class IRHIDevice;       }
namespace Renderer       { class Renderer;         }
namespace HedgehogEngine { class HedgehogEngine;   }

namespace HedgehogGui
{
    // Full-featured editor GUI node: owns the ImGui Vulkan backend, all editor
    // panels, and the ImGui frame lifecycle. Append to Renderer::AppendNode
    // before the first DrawFrame.
    class EditorGuiNode final : public Renderer::IRenderNode
    {
    public:
        EditorGuiNode(HW::Window&                     window,
                      Renderer::Renderer&              renderer,
                      HedgehogEngine::HedgehogEngine&  engine);
        ~EditorGuiNode() override;

        EditorGuiNode(const EditorGuiNode&)            = delete;
        EditorGuiNode& operator=(const EditorGuiNode&) = delete;

        void  OnBeginFrame()                                    override;
        void  OnDiscardFrame()                                  override;
        void  PreRender(const Renderer::PreRenderContext& ctx)  override;
        void  Render(Renderer::RenderContext& ctx)              override;
        void  Cleanup(RHI::IRHIDevice& device)                  override;
        void* ExportResource(const std::string& key) const      override;

        bool IsSceneViewHovered() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_Impl;
    };

} // namespace HedgehogGui
