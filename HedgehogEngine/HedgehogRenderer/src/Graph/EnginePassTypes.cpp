#include "HedgehogRenderer/Graph/EnginePassTypes.hpp"

#include "HedgehogRenderer/Graph/GraphAssetVocabulary.hpp"
#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"

#include <cassert>

namespace Renderer
{
    namespace
    {
        struct TargetPassData
        {
            RGTexture Target{};
        };

        struct ForwardPassData
        {
            RGTexture Color{};
            bool      CullBackFaces = true;
        };

        // Declares a pass whose only effect is writing one slot as a depth or color target.
        template<RGTexture (RGPassBuilder::*Write)(RGTexture)>
        void BuildSingleTargetPass(RenderGraphRuntime& graph, PassInvocation& invocation, const char* slot)
        {
            graph.AddPass<TargetPassData>(invocation.GetName(),
                [&](RGPassBuilder& pass, TargetPassData& data)
                {
                    data.Target = (pass.*Write)(invocation.GetSlot(slot));
                    invocation.SetSlot(slot, data.Target);
                },
                [](TargetPassData&, RHI::IRHICommandList&) {});
        }

        void BuildDepthPrepass(RenderGraphRuntime& graph, PassInvocation& invocation)
        {
            BuildSingleTargetPass<&RGPassBuilder::DepthTarget>(graph, invocation, "depth");
        }

        void BuildShadow(RenderGraphRuntime& graph, PassInvocation& invocation)
        {
            BuildSingleTargetPass<&RGPassBuilder::DepthTarget>(graph, invocation, "shadowMap");
        }

        void BuildUi(RenderGraphRuntime& graph, PassInvocation& invocation)
        {
            BuildSingleTargetPass<&RGPassBuilder::ColorTarget>(graph, invocation, "target");
        }

        void BuildForward(RenderGraphRuntime& graph, PassInvocation& invocation)
        {
            // The instantiator has already checked the value is a flag; a C++ caller may omit it.
            const std::optional<bool> cull = ResolveFlag(invocation.GetParameter("cullBackFaces").value_or("true"));
            assert(cull && "Forward: cullBackFaces must be 'true' or 'false'.");

            graph.AddPass<ForwardPassData>(invocation.GetName(),
                [&](RGPassBuilder& pass, ForwardPassData& data)
                {
                    pass.DepthReadOnly(invocation.GetSlot("depth"));
                    pass.SampleTexture(invocation.GetSlot("shadowMap"));
                    data.Color         = pass.ColorTarget(invocation.GetSlot("color"));
                    data.CullBackFaces = cull.value_or(true);
                    invocation.SetSlot("color", data.Color);
                },
                [](ForwardPassData&, RHI::IRHICommandList&) {});
        }
    }

    void RegisterEnginePassTypes(PassBuilderRegistry& registry)
    {
        [[maybe_unused]] const bool registered =
            registry.Register("DepthPrepass", { { "depth" }, {}, &BuildDepthPrepass })
            && registry.Register("Shadow", { { "shadowMap" }, {}, &BuildShadow })
            && registry.Register("Forward", { { "color", "depth", "shadowMap" },
                                              { { "cullBackFaces", PassParameterKind::Flag } }, &BuildForward })
            && registry.Register("Ui", { { "target" }, {}, &BuildUi });
        assert(registered && "RegisterEnginePassTypes: an engine pass type was already registered.");
    }
}
