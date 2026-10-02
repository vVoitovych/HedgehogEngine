#include "GameUiPass.hpp"

#include "PassCommon.hpp"

#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHITexture.hpp"

#include <algorithm>
#include <cmath>

namespace Renderer
{
    namespace
    {
        // The UI's target size, or the colour target's when the frame names none.
        HM::Vector2 UiSizeOr(const HM::Vector2& uiTargetSize, uint32_t width, uint32_t height)
        {
            if (uiTargetSize.x() > 0.0f && uiTargetSize.y() > 0.0f)
                return uiTargetSize;
            return HM::Vector2(static_cast<float>(width), static_cast<float>(height));
        }

        // The set a command samples: its font's (nullptr when the font has none, and the command is
        // skipped), its texture's, or the white texture for a solid fill or a texture that did not load.
        const RHI::IRHIDescriptorSet* TextureOf(const GraphFrameData& frame, uint32_t texture)
        {
            if (texture != HX::UI_NO_TEXTURE && (texture & HX::UI_FONT_TEXTURE) != 0)
            {
                const uint32_t font = texture & ~HX::UI_FONT_TEXTURE;
                return font < frame.UiFontSets.size() ? frame.UiFontSets[font] : nullptr;
            }
            if (texture < frame.UiTextureSets.size() && frame.UiTextureSets[texture])
                return frame.UiTextureSets[texture];
            return frame.UiSolidTexture;
        }

        void RecordGameUi(TargetPassData& data, RHI::IRHICommandList& cmd)
        {
            if (!data.Context)
                return;
            const GraphFrameData& frame = *data.Context->Frame;
            if (frame.UiCommands.empty() || !frame.UiVertices || !frame.UiIndices || !frame.UiSolidTexture)
                return;

            IGraphPassServices&      services = *data.Context->Services;
            const RHI::IRHIPipeline& pipeline = services.GetPipeline(EnginePipeline::GameUi);
            RHI::IRHITexture&        color    = *data.Graph->GetTexture(data.Target);
            const uint32_t           width    = color.GetWidth();
            const uint32_t           height   = color.GetHeight();
            const HM::Vector2        uiSize   = UiSizeOr(frame.UiTargetSize, width, height);

            RHI::RenderingAttachment colorAttachment;
            colorAttachment.Texture = &color;
            colorAttachment.LoadOp  = RHI::LoadOp::Load;
            colorAttachment.StoreOp = RHI::StoreOp::Store;

            RHI::RenderingInfo info;
            info.ColorAttachments = { colorAttachment };
            info.Width            = width;
            info.Height           = height;
            cmd.BeginRendering(info);

            cmd.BindPipeline(pipeline);
            cmd.SetViewport({ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f });
            cmd.BindVertexBuffers(0, { frame.UiVertices }, { 0 });
            cmd.BindIndexBuffer(*frame.UiIndices, RHI::IndexType::Uint16);
            const GameUiPushConstants constants = MakeGameUiPushConstants(uiSize);
            cmd.PushConstants(pipeline, RHI::ShaderStage::Vertex, 0, sizeof(constants), &constants);

            for (const HX::UiDrawCommand& command : frame.UiCommands)
            {
                const RHI::Scissor            scissor = MakeGameUiScissor(command.Scissor, uiSize, width, height);
                const RHI::IRHIDescriptorSet* texture = TextureOf(frame, command.Texture);
                if (command.IndexCount == 0 || scissor.Width == 0 || scissor.Height == 0 || !texture)
                    continue;

                cmd.SetScissor(scissor);
                cmd.BindDescriptorSet(pipeline, 0, *texture);
                cmd.DrawIndexed(command.IndexCount, 1, command.FirstIndex, static_cast<int32_t>(command.VertexOffset), 0);
            }
            cmd.EndRendering();
        }

        void BuildGameUi(RenderGraphRuntime& graph, PassInvocation& invocation)
        {
            graph.AddPass<TargetPassData>(invocation.GetName(),
                [&](RGPassBuilder& pass, TargetPassData& data)
                {
                    data.Target  = pass.ColorTarget(invocation.GetSlot("color"));
                    data.Graph   = &graph;
                    data.Context = graph.GetFrameContext();
                    invocation.SetSlot("color", data.Target);
                },
                [](TargetPassData& data, RHI::IRHICommandList& cmd) { RecordGameUi(data, cmd); });
        }
    }

    GameUiPushConstants MakeGameUiPushConstants(const HM::Vector2& uiTargetSize)
    {
        GameUiPushConstants constants;
        if (uiTargetSize.x() > 0.0f && uiTargetSize.y() > 0.0f)
        {
            constants.Scale[0]  = 2.0f / uiTargetSize.x();
            constants.Scale[1]  = 2.0f / uiTargetSize.y();
            constants.Offset[0] = -1.0f;
            constants.Offset[1] = -1.0f;
        }
        return constants;
    }

    RHI::Scissor MakeGameUiScissor(const HX::UiRect& scissor, const HM::Vector2& uiTargetSize, uint32_t width,
                                   uint32_t height)
    {
        const HM::Vector2 uiSize = UiSizeOr(uiTargetSize, width, height);
        const float       sx     = static_cast<float>(width) / uiSize.x();
        const float       sy     = static_cast<float>(height) / uiSize.y();

        const float left   = std::clamp(std::floor(scissor.X * sx), 0.0f, static_cast<float>(width));
        const float top    = std::clamp(std::floor(scissor.Y * sy), 0.0f, static_cast<float>(height));
        const float right  = std::clamp(std::ceil((scissor.X + scissor.Width) * sx), 0.0f, static_cast<float>(width));
        const float bottom = std::clamp(std::ceil((scissor.Y + scissor.Height) * sy), 0.0f, static_cast<float>(height));

        RHI::Scissor result;
        result.X      = static_cast<int32_t>(left);
        result.Y      = static_cast<int32_t>(top);
        result.Width  = right > left ? static_cast<uint32_t>(right - left) : 0u;
        result.Height = bottom > top ? static_cast<uint32_t>(bottom - top) : 0u;
        return result;
    }

    PassTypeInfo GetGameUiPassType()
    {
        return { { "color" }, {}, &BuildGameUi };
    }
}
