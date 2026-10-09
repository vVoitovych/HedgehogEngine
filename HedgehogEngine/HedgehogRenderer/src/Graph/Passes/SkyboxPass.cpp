#include "SkyboxPass.hpp"

#include "PassCommon.hpp"

#include "HedgehogRenderer/Graph/EnvironmentUniform.hpp"

#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHITexture.hpp"

#include <algorithm>

namespace Renderer
{
    namespace
    {
        // The vertices of the fullscreen triangle Skybox/Sky.vert makes from gl_VertexIndex.
        constexpr uint32_t FULLSCREEN_TRIANGLE_VERTICES = 3;

        void RecordSkybox(ForwardPassData& data, RHI::IRHICommandList& cmd)
        {
            if (!data.Context)
                return;
            const GraphFrameData& frame = *data.Context->Frame;
            if (!frame.Environment.ShowSkybox || !frame.Environment.Radiance)
                return;

            IGraphPassServices&           services = *data.Context->Services;
            const RHI::IRHIPipeline&      pipeline = services.GetPipeline(EnginePipeline::Skybox);
            const RHI::IRHIDescriptorSet& cube     = services.AllocateSampledTexture(*frame.Environment.Radiance);
            RHI::IRHITexture&             color    = *data.Graph->GetTexture(data.Color);
            RHI::IRHITexture&             depth    = *data.Graph->GetTexture(data.Depth);

            RHI::RenderingAttachment colorAttachment;
            colorAttachment.Texture = &color;
            colorAttachment.LoadOp  = RHI::LoadOp::Load;
            colorAttachment.StoreOp = RHI::StoreOp::Store;

            RHI::RenderingAttachment depthAttachment;
            depthAttachment.Texture = &depth;
            depthAttachment.LoadOp  = RHI::LoadOp::Load;
            depthAttachment.StoreOp = RHI::StoreOp::Store;

            RHI::RenderingInfo info;
            info.ColorAttachments = { colorAttachment };
            info.DepthAttachment  = depthAttachment;
            info.Width            = color.GetWidth();
            info.Height           = color.GetHeight();
            cmd.BeginRendering(info);

            cmd.BindPipeline(pipeline);
            cmd.SetViewport({ 0.0f, 0.0f, static_cast<float>(color.GetWidth()), static_cast<float>(color.GetHeight()),
                              0.0f, 1.0f });
            cmd.SetScissor({ 0, 0, color.GetWidth(), color.GetHeight() });
            cmd.BindDescriptorSet(pipeline, 0, cube);
            const SkyboxPushConstants constants = MakeSkyboxPushConstants(frame.View, frame.Proj, frame.Environment.Uniform);
            cmd.PushConstants(pipeline, RHI::ShaderStage::Fragment, 0, sizeof(constants), &constants);
            cmd.Draw(FULLSCREEN_TRIANGLE_VERTICES, 1, 0, 0);
            cmd.EndRendering();
        }

        void BuildSkybox(RenderGraphRuntime& graph, PassInvocation& invocation)
        {
            graph.AddPass<ForwardPassData>(invocation.GetName(),
                [&](RGPassBuilder& pass, ForwardPassData& data)
                {
                    data.Depth = invocation.GetSlot("depth");
                    pass.DepthReadOnly(data.Depth);
                    data.Color   = pass.ColorTarget(invocation.GetSlot("color"));
                    data.Graph   = &graph;
                    data.Context = graph.GetFrameContext();
                    invocation.SetSlot("color", data.Color);
                },
                [](ForwardPassData& data, RHI::IRHICommandList& cmd) { RecordSkybox(data, cmd); });
        }
    }

    SkyboxPushConstants MakeSkyboxPushConstants(const HM::Matrix4x4& view, const HM::Matrix4x4& proj,
                                                const EnvironmentUniform& environment)
    {
        // Column 3 holds the translation (Matrix4x4 stores columns, as GLSL does).
        HM::Matrix4x4 rotationOnly = view;
        rotationOnly[3]            = HM::Vector4(0.0f, 0.0f, 0.0f, 1.0f);
        const HM::Matrix4x4 inverse = (proj * rotationOnly).Inverse();

        SkyboxPushConstants constants;
        std::copy_n(inverse.GetBuffer(), 16, constants.InverseViewProj);
        constants.RotationCos = environment.RotationCos;
        constants.RotationSin = environment.RotationSin;
        constants.Intensity   = environment.Intensity;
        return constants;
    }

    PassTypeInfo GetSkyboxPassType()
    {
        return { { "color", "depth" }, {}, &BuildSkybox };
    }
}
