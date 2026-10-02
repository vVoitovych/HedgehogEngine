#include "HedgehogRenderer/Frame/SharedPhase.hpp"

#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"

#include <cassert>

namespace Renderer
{
    namespace
    {
        constexpr RHI::Format SHADOW_ATLAS_FORMAT = RHI::Format::D32Float;
        constexpr uint32_t    LAYER_COUNT         = 32;

        bool CastsShadow(const HX::RenderInstance& instance, uint32_t casterMask)
        {
            return instance.Layer < LAYER_COUNT && (casterMask & (1u << instance.Layer)) != 0;
        }
    }

    SharedPhaseOutputs SharedPhase::Declare(RenderGraphRuntime& graph, IGraphPassServices& services,
                                            const GraphFrameData* shadowView,
                                            std::span<const HX::RenderLight> lights,
                                            const SharedPhaseSettings& settings)
    {
        SharedPhaseOutputs outputs;
        outputs.SceneLights = &services.AllocateSceneLightsUniform(MakeSceneLightsUniform(lights));
        if (!shadowView)
            return outputs;

        // Casters come from the caster mask alone, never from the view's layer mask, and are split
        // rigid and skinned as a view's instances are, from either of shadowView's lists.
        m_Casters.clear();
        m_SkinnedCasters.clear();
        for (const std::span<const HX::RenderInstance> instances :
             { shadowView->OpaqueInstances, shadowView->SkinnedInstances })
        {
            for (const HX::RenderInstance& instance : instances)
            {
                if (CastsShadow(instance, settings.ShadowCasterMask))
                    (instance.JointCount > 0 ? m_SkinnedCasters : m_Casters).push_back(instance);
            }
        }
        m_ShadowFrame                  = *shadowView;
        m_ShadowFrame.OpaqueInstances  = m_Casters;
        m_ShadowFrame.SkinnedInstances = m_SkinnedCasters;
        m_ShadowContext               = { &services, &m_ShadowFrame };

        const RGSizePolicy size = RGSizePolicy::MakeAbsolute(settings.ShadowAtlasSize, settings.ShadowAtlasSize);
        PassInvocation     shadow("Shadow");
        shadow.SetSlot("shadowMap", graph.CreateTexture({ SHADOW_ATLAS_IMPORT, SHADOW_ATLAS_FORMAT, size,
                                                          DefaultTextureUsage(SHADOW_ATLAS_FORMAT) }));

        const PassTypeInfo* shadowType = m_Registry.Find("Shadow");
        assert(shadowType && "SharedPhase: the Shadow pass type is not registered (RegisterEnginePassTypes).");
        const GraphFrameContext* callerContext = graph.GetFrameContext();
        graph.SetFrameContext(&m_ShadowContext);
        shadowType->Build(graph, shadow);
        graph.SetFrameContext(callerContext);

        outputs.Imports.emplace(SHADOW_ATLAS_IMPORT, shadow.GetSlot("shadowMap"));
        return outputs;
    }

    const View* SelectShadowView(std::span<const View> views)
    {
        const View* selected = nullptr;
        for (const View& view : views)
        {
            if (view.Desc.Camera && (!selected || view.Desc.Priority > selected->Desc.Priority))
                selected = &view;
        }
        return selected;
    }
}
