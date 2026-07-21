#pragma once

#include "HedgehogCommon/api/Frame/FrameData.hpp"
#include "HedgehogCommon/api/RendererSettings.hpp"

#include "HedgehogMath/api/Matrix.hpp"
#include "HedgehogMath/api/Vector.hpp"

#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace RHI
{
    class IRHIDevice;
    class IRHICommandList;
    class IRHIRenderPass;
    class IRHIPipeline;
    class IRHIFramebuffer;
    class IRHIBuffer;
}

namespace FS
{
    class FileSystemManager;
}

namespace Renderer
{
    class ResourceManager;

    // Editor overlay pass: draws line gizmos (selected-entity transform axes + oriented box,
    // and light glyphs) on top of the scene colour target. Scene view only; never the game view.
    class GizmoPass
    {
    public:
        GizmoPass(RHI::IRHIDevice& device, const ResourceManager& resourceManager,
                  const FS::FileSystemManager& fileSystem);
        ~GizmoPass();

        void Render(const HedgehogEngine::CameraData&              camera,
                    const std::vector<HedgehogEngine::LightData>&  lights,
                    const std::optional<HM::Matrix4x4>&            selected,
                    RHI::IRHICommandList& cmd, uint32_t frameIndex);
        void Cleanup(RHI::IRHIDevice& device);

        void ResizeResources(RHI::IRHIDevice& device, const ResourceManager& resourceManager);

    private:
        // Tightly packed (24 bytes) to match PositionColor.vdes: vec3 position + vec3 colour.
        struct GizmoVertex { float px, py, pz, cx, cy, cz; };

        void CreateFramebuffer(RHI::IRHIDevice& device, const ResourceManager& resourceManager);
        void BuildLines(const std::vector<HedgehogEngine::LightData>& lights,
                        const std::optional<HM::Matrix4x4>&           selected);

    private:
        std::unique_ptr<RHI::IRHIRenderPass>  m_RenderPass;
        std::unique_ptr<RHI::IRHIPipeline>    m_Pipeline;
        std::unique_ptr<RHI::IRHIFramebuffer> m_FrameBuffer;

        std::array<std::unique_ptr<RHI::IRHIBuffer>, HedgehogEngine::MAX_FRAMES_IN_FLIGHT> m_VertexBuffers;
        std::vector<GizmoVertex> m_Lines; // reused CPU scratch, rebuilt each frame
    };
}
