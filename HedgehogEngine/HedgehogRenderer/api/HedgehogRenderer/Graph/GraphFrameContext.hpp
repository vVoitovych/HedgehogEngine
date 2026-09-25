#pragma once

#include "HedgehogCommon/api/RendererSettings.hpp"
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
        HM::Vector3   EyePosition = HM::Vector3(0.0f, 0.0f, 0.0f);
        float         NearPlane = 0.1f;
        float         FarPlane  = 1000.0f;

        // The shadow-casting light's direction; the shadow pass falls back to +X without one.
        std::optional<HM::Vector3> ShadowLightDirection;
        uint32_t                   ShadowCascadeCount       = 1;
        float                      ShadowCascadeSplitLambda = 0.5f;

        // Opaque instances only: what the depth prepass, shadow and forward passes draw.
        std::span<const HX::RenderInstance> OpaqueInstances;

        // The lights the forward pass shades with; at most MAX_LIGHTS_COUNT are used.
        std::span<const HX::RenderLight> Lights;

        // Shared geometry. Meshes is indexed by RenderInstance::MeshIndex. TexCoords and Normals
        // are only read by the forward pass.
        RHI::IRHIBuffer*              Positions = nullptr;
        RHI::IRHIBuffer*              TexCoords = nullptr;
        RHI::IRHIBuffer*              Normals   = nullptr;
        RHI::IRHIBuffer*              Indices   = nullptr;
        std::span<const MeshDrawRange> Meshes;

        // Each material's descriptor set (the forward shader's set 1), indexed by
        // RenderInstance::MaterialIndex. Owned by the resource registry.
        std::span<const RHI::IRHIDescriptorSet* const> MaterialSets;
    };

    // The forward shader's set 0 (ForwardPass/Base.vert and .frag), laid out for std140.
    struct GpuLight
    {
        alignas(16) HM::Vector3 Position;
        alignas(16) HM::Vector3 Direction;
        alignas(16) HM::Vector3 Color;
        alignas(16) HM::Vector4 Data; // (type, intensity, radius, cos(coneAngle))
    };

    struct ForwardViewUniform
    {
        alignas(16) HM::Matrix4x4 View;
        alignas(16) HM::Matrix4x4 ViewProj;
        alignas(16) HM::Vector3   EyePosition;
        alignas(16) GpuLight      Lights[HedgehogEngine::MAX_LIGHTS_COUNT];
        int32_t                   LightCount = 0;
    };

    // Packs one view's camera and lights into the forward shader's layout. Lights past
    // MAX_LIGHTS_COUNT are dropped, as the legacy forward pass did.
    [[nodiscard]] ForwardViewUniform MakeForwardViewUniform(const GraphFrameData& frame);

    enum class EnginePipeline
    {
        DepthPrepass,
        Shadow,
        Forward,            // back faces culled
        ForwardDoubleSided, // Forward with cullBackFaces: false
    };

    // The long-lived GPU objects the engine passes use but do not own: pipelines, and per-frame
    // rings of uniform buffers. Implemented by GraphPassServices (outside src/Graph, so the headless
    // tests never need a device); tests substitute a fake.
    class IGraphPassServices
    {
    public:
        virtual ~IGraphPassServices() = default;

        virtual const RHI::IRHIPipeline& GetPipeline(EnginePipeline pipeline) const = 0;

        // A descriptor set (set 0, binding 0) holding viewProj, valid until this frame slot comes
        // round again. One allocation per draw pass (or per shadow cascade) per frame.
        virtual const RHI::IRHIDescriptorSet& AllocateViewProjUniform(const HM::Matrix4x4& viewProj) = 0;

        // The forward pass's set 0: camera and lights for one view. One allocation per forward pass
        // per frame, from its own per-frame ring.
        virtual const RHI::IRHIDescriptorSet& AllocateForwardViewUniform(const ForwardViewUniform& uniform) = 0;
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
