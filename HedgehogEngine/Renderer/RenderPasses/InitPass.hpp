#pragma once

namespace Renderer
{
    class RenderContext;

    class InitPass
    {
    public:
        InitPass(const RenderContext& context);
        ~InitPass() = default;

        void Render(RenderContext& context);

        void Cleanup(const RenderContext& context);

    };

}

