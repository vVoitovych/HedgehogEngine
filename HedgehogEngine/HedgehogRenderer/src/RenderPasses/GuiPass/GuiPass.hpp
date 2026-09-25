#pragma once

#include "HedgehogRenderer/Graph/UiCallback.hpp"

namespace RHI
{
    class IRHICommandList;
}

namespace Renderer
{
    class ResourceManager;

    // The legacy path's UI step: the application's UiCallback records into the legacy colour
    // buffer, as the render graph's Ui pass does into main. The renderer owns no UI library.
    class GuiPass
    {
    public:
        void Render(RHI::IRHICommandList& cmd, const ResourceManager& resourceManager, const UiCallback& ui);
    };
}
