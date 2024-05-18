#pragma once

#include <vulkan/vulkan.h>
#include <optional>
#include <memory>

namespace Renderer
{
    class RenderContext;
    class ResourceManager;

    class PresentPass
    {
    public:
        PresentPass(const std::unique_ptr<RenderContext>& context);
        ~PresentPass() = default;

        void Render(std::unique_ptr<RenderContext>& context, const std::unique_ptr<ResourceManager>& resourceManager);

        void Cleanup(const std::unique_ptr<RenderContext>& context);

    };


}


