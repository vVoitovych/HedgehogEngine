#pragma once

#include "BaseRenderPass.hpp"

namespace Renderer
{
    class Device;
    class RenderContext;
    class ResourceTracker;

    class PresentPass : public BaseRenderPass
    {
    public:
        PresentPass() = default;
        virtual ~PresentPass() = default;

        virtual void Render(RenderContext& renderContext, ResourceTracker& resourceTracker);
        virtual void Initialize(Device& device, RenderContext& renderContext);
        virtual void Cleanup(Device& device);

    };


}


