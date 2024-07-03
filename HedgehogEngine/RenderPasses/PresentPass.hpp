#pragma once

#include <vulkan/vulkan.h>

namespace Renderer
{
    class RenderContext;
    class ResourceManager;

    class PresentPass
    {
    public:
        PresentPass(const RenderContext& context);
        ~PresentPass() = default;

        void Render(RenderContext& context, const ResourceManager& resourceManager);

        void Cleanup(const RenderContext& context);

    };


}


