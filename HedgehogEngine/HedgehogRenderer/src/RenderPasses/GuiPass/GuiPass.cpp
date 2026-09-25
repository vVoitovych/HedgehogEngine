#include "GuiPass.hpp"

#include "ResourceManager/ResourceManager.hpp"

#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHITexture.hpp"
#include "RHI/api/RHITypes.hpp"

namespace Renderer
{
    void GuiPass::Render(RHI::IRHICommandList& cmd, const ResourceManager& resourceManager, const UiCallback& ui)
    {
        // The UI covers the whole buffer, so its previous contents are not needed.
        auto& color = const_cast<RHI::IRHITexture&>(resourceManager.GetRHIColorBuffer());
        cmd.TransitionTexture(color, RHI::ImageLayout::Undefined, RHI::ImageLayout::ColorAttachment);
        if (ui)
        {
            ui(cmd, color);
            return;
        }

        RHI::RenderingAttachment clear;
        clear.Texture     = &color;
        clear.LoadOp      = RHI::LoadOp::Clear;
        clear.StoreOp     = RHI::StoreOp::Store;
        clear.Clear.Color = { 0.0f, 0.0f, 0.0f, 1.0f };

        RHI::RenderingInfo info;
        info.ColorAttachments = { clear };
        info.Width            = color.GetWidth();
        info.Height           = color.GetHeight();
        cmd.BeginRendering(info);
        cmd.EndRendering();
    }
}
