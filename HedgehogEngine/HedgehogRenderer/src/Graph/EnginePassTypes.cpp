#include "HedgehogRenderer/Graph/EnginePassTypes.hpp"

#include "Passes/DepthPrepassPass.hpp"
#include "Passes/ForwardPass.hpp"
#include "Passes/GameUiPass.hpp"
#include "Passes/GizmoPass.hpp"
#include "Passes/ShadowPass.hpp"
#include "Passes/ToneMapPass.hpp"
#include "Passes/UiPass.hpp"

#include <cassert>

namespace Renderer
{
    // Each pass type lives in its own pair of files under Passes/, which declares its slots and
    // parameters and records it; the helpers they share are in Passes/PassCommon.
    void RegisterEnginePassTypes(PassBuilderRegistry& registry)
    {
        [[maybe_unused]] const bool registered =
            registry.Register("DepthPrepass", GetDepthPrepassPassType())
            && registry.Register("Shadow", GetShadowPassType())
            && registry.Register("Forward", GetForwardPassType())
            && registry.Register("ToneMap", GetToneMapPassType())
            && registry.Register("Gizmo", GetGizmoPassType())
            && registry.Register("GameUi", GetGameUiPassType())
            && registry.Register("Ui", GetUiPassType());
        assert(registered && "RegisterEnginePassTypes: an engine pass type was already registered.");
    }
}
