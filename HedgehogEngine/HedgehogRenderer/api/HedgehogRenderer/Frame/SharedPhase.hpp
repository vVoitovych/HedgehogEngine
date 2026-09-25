#pragma once

#include "HedgehogRenderer/Graph/GraphFrameContext.hpp"
#include "HedgehogRenderer/Graph/GraphInstantiator.hpp"
#include "HedgehogRenderer/Graph/PassBuilderRegistry.hpp"
#include "HedgehogRenderer/Views/View.hpp"

#include "HedgehogExtract/api/RenderScene.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace RHI
{
    class IRHIDescriptorSet;
}

// RENDERING.md section 3.4: work that is not per view runs once per frame, before any view's graph.
//
// The shared phase declares its passes into the same RenderGraphRuntime the views' graphs are then
// instantiated into, and hands those graphs its outputs as imports (GraphImports). Its outputs are
// therefore ordinary graph resources: the compiler orders the shared passes before every view pass
// that reads them, derives the barriers, and culls a shared pass no surviving view reads. Nothing
// about the order is hardcoded.
namespace Renderer
{
    class RenderGraphRuntime;

    // The name view graphs import the shadow atlas under (scene.graph, game.graph).
    inline constexpr const char* SHADOW_ATLAS_IMPORT = "shadowAtlas";

    struct SharedPhaseSettings
    {
        uint32_t ShadowAtlasSize  = 2048;
        // Which layers cast shadows (HedgehogSettings::ShadowmapSettings). Never a view's mask: an
        // object on a layer a view hides still shadows what the view shows.
        uint32_t ShadowCasterMask = 0xFFFFFFFFu;
    };

    struct SharedPhaseOutputs
    {
        GraphImports                  Imports;               // pass to GraphInstantiator::Instantiate
        const RHI::IRHIDescriptorSet* SceneLights = nullptr; // set as every view's GraphFrameData::SceneLights
    };

    class SharedPhase
    {
    public:
        explicit SharedPhase(const PassBuilderRegistry& registry) : m_Registry(registry) {}

        SharedPhase(const SharedPhase&)            = delete;
        SharedPhase& operator=(const SharedPhase&) = delete;
        SharedPhase(SharedPhase&&)                 = delete;
        SharedPhase& operator=(SharedPhase&&)      = delete;

        // Declares this frame's shared work into graph; call before instantiating any view's graph.
        //
        //  - Scene upload: packs lights into one uniform for every view (services' ring).
        //  - Shadow atlas: one Shadow pass into a D32Float atlas, its cascades fitted to shadowView's
        //    camera, drawing every opaque instance on a caster layer. shadowView is the primary view's
        //    frame data (SelectShadowView); with none, no atlas is declared and nothing is imported.
        //
        // The shadow pass captures this object's frame context, so it must outlive graph's Execute().
        // The caller's own frame context is restored before returning.
        [[nodiscard]] SharedPhaseOutputs Declare(RenderGraphRuntime& graph, IGraphPassServices& services,
                                                 const GraphFrameData* shadowView,
                                                 std::span<const HX::RenderLight> lights,
                                                 const SharedPhaseSettings& settings);

    private:
        const PassBuilderRegistry&      m_Registry;
        std::vector<HX::RenderInstance> m_Casters;     // reused: steady-state frames allocate nothing
        GraphFrameData                  m_ShadowFrame; // shadowView with only the casters
        GraphFrameContext               m_ShadowContext;
    };

    // The view whose camera the shared shadow cascades are fitted to: the highest-Priority view with
    // a camera, the earliest in `views` on a tie. nullptr when no view has a camera.
    [[nodiscard]] const View* SelectShadowView(std::span<const View> views);
}
