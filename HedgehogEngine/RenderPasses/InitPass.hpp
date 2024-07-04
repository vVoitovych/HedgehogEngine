#pragma once

namespace Context
{
    class Context;
}

namespace Renderer
{
    class InitPass
    {
    public:
        InitPass(const Context::Context& context);
        ~InitPass() = default;

        void Render(Context::Context& context);

        void Cleanup(const Context::Context& context);

    };

}

