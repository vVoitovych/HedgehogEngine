#pragma once

#include <vulkan/vulkan.h>

namespace Context
{
    class Context;
}

namespace Renderer
{
    class ResourceManager;

    class PresentPass
    {
    public:
        PresentPass(const Context::Context& context);
        ~PresentPass() = default;

        void Render(Context::Context& context, const ResourceManager& resourceManager);

        void Cleanup(const Context::Context& context);

    };


}


