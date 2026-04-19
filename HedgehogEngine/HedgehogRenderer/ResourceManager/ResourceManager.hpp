#pragma once

#include <memory>

namespace RHI
{
    class IRHITexture;
}

namespace Context
{
    class Context;
}

namespace Renderer
{
    class ResourceManager
    {
    public:
        ResourceManager(const Context::Context& context);
        ~ResourceManager();

        void Cleanup(const Context::Context& context);

        void ResizeFrameBufferSizeDependenteResources(const Context::Context& context);
        void ResizeSettingsDependenteResources(const Context::Context& context);

        const RHI::IRHITexture& GetRHIColorBuffer() const;
        const RHI::IRHITexture& GetRHIDepthBuffer() const;
        const RHI::IRHITexture& GetRHIShadowMap() const;
        const RHI::IRHITexture& GetRHIShadowMask() const;

    private:
        void CreateRHIDepthBuffer(const Context::Context& context);
        void CreateRHIColorBuffer(const Context::Context& context);
        void CreateRHIShadowMap(const Context::Context& context);
        void CreateRHIShadowMask(const Context::Context& context);

    private:
        std::unique_ptr<RHI::IRHITexture> m_RHIDepthBuffer;
        std::unique_ptr<RHI::IRHITexture> m_RHIColorBuffer;
        std::unique_ptr<RHI::IRHITexture> m_RHIShadowMap;
        std::unique_ptr<RHI::IRHITexture> m_RHIShadowMask;
    };
}
