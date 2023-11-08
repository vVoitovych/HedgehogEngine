#pragma once

namespace Renderer
{
    class Device;
    class RenderContext;
    class ResourceTracker;

    class BaseRenderPass
    {
    public:
        BaseRenderPass() = default;
        virtual ~BaseRenderPass() = default;

        virtual void Render(RenderContext& renderContext, ResourceTracker& resourceTracker) = 0;

        virtual void Initialize(Device& device, RenderContext& renderContext) = 0;
        virtual void Cleanup(Device& device) = 0;
    };
}



