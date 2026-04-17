#pragma once

#include <memory>
#include <vector>

namespace Wrappers
{
    class Image;
}

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

        // Legacy Wrappers API (kept until render passes are migrated).
        const Wrappers::Image& GetColorBuffer() const;
        const Wrappers::Image& GetDepthBuffer() const;
        const Wrappers::Image& GetShadowMap() const;
        const Wrappers::Image& GetShadowMask() const;

        // New RHI API.
        const RHI::IRHITexture& GetRHIColorBuffer() const;
        const RHI::IRHITexture& GetRHIDepthBuffer() const;
        const RHI::IRHITexture& GetRHIShadowMap() const;
        const RHI::IRHITexture& GetRHIShadowMask() const;

    private:
        void CreateDepthBuffer(const Context::Context& context);
        void CreateColorBuffer(const Context::Context& context);
        void CreateShadowMap(const Context::Context& context);
        void CreateShadowMask(const Context::Context& context);

        void CreateRHIDepthBuffer(const Context::Context& context);
        void CreateRHIColorBuffer(const Context::Context& context);
        void CreateRHIShadowMap(const Context::Context& context);
        void CreateRHIShadowMask(const Context::Context& context);

    private:
        // Legacy Wrappers objects (removed once render passes are migrated).
        std::unique_ptr<Wrappers::Image> m_DepthBuffer;
        std::unique_ptr<Wrappers::Image> m_ColorBuffer;
        std::unique_ptr<Wrappers::Image> m_ShadowMap;
        std::unique_ptr<Wrappers::Image> m_ShadowMask;

        // RHI objects — authoritative going forward.
        std::unique_ptr<RHI::IRHITexture> m_RHIDepthBuffer;
        std::unique_ptr<RHI::IRHITexture> m_RHIColorBuffer;
        std::unique_ptr<RHI::IRHITexture> m_RHIShadowMap;
        std::unique_ptr<RHI::IRHITexture> m_RHIShadowMask;
    };
}



