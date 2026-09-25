#pragma once

#include "HedgehogExtract/api/RenderScene.hpp"
#include "HedgehogMath/api/Matrix.hpp"
#include "HedgehogMath/api/Vector.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace RHI
{
    class IRHIBuffer;
    class IRHIDescriptorSet;
    class IRHIPipeline;
}

namespace Renderer
{
    // Where one mesh's indices sit in the shared geometry buffers.
    struct MeshDrawRange
    {
        uint32_t FirstIndex   = 0;
        uint32_t IndexCount   = 0;
        uint32_t VertexOffset = 0;
    };

    // Everything the engine passes read about one view's frame, filled by the frame loop. Plain
    // data: no pass owns it, and nothing here outlives the frame.
    struct GraphFrameData
    {
        HM::Matrix4x4 View;
        HM::Matrix4x4 Proj;
        float         NearPlane = 0.1f;
        float         FarPlane  = 1000.0f;

        // The shadow-casting light's direction; the shadow pass falls back to +X without one.
        std::optional<HM::Vector3> ShadowLightDirection;
        uint32_t                   ShadowCascadeCount       = 1;
        float                      ShadowCascadeSplitLambda = 0.5f;

        // Opaque instances only: what the depth prepass and the shadow pass draw.
        std::span<const HX::RenderInstance> OpaqueInstances;

        // Shared geometry. Meshes is indexed by RenderInstance::MeshIndex.
        RHI::IRHIBuffer*              Positions = nullptr;
        RHI::IRHIBuffer*              Indices   = nullptr;
        std::span<const MeshDrawRange> Meshes;
    };

    enum class EnginePipeline
    {
        DepthPrepass,
        Shadow,
    };

    // The long-lived GPU objects the engine passes use but do not own: pipelines, and a per-frame
    // ring of uniform buffers. Implemented by GraphPassServices (outside src/Graph, so the headless
    // tests never need a device); tests substitute a fake.
    class IGraphPassServices
    {
    public:
        virtual ~IGraphPassServices() = default;

        virtual const RHI::IRHIPipeline& GetPipeline(EnginePipeline pipeline) const = 0;

        // A descriptor set (set 0, binding 0) holding viewProj, valid until this frame slot comes
        // round again. One allocation per draw pass (or per shadow cascade) per frame.
        virtual const RHI::IRHIDescriptorSet& AllocateViewProjUniform(const HM::Matrix4x4& viewProj) = 0;
    };

    // Attached to a RenderGraphRuntime for one frame (SetFrameContext). Pass builders capture a
    // pointer to it in their arena pass data; with none attached, as in the headless tests, their
    // execute bodies record nothing, while their declarations are unchanged.
    struct GraphFrameContext
    {
        IGraphPassServices*   Services = nullptr;
        const GraphFrameData* Frame    = nullptr;
    };
}
