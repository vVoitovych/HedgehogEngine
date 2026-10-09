#pragma once

#include "HedgehogRenderer/Graph/EnvironmentUniform.hpp"

#include "ContentLoader/api/LoadedEnvironment.hpp"

#include <memory>
#include <string>
#include <unordered_map>

namespace RHI
{
    class IRHIDevice;
    class IRHITexture;
}

namespace FS
{
    class FileSystemManager;
}

namespace HX
{
    struct RenderEnvironment;
}

namespace Renderer
{
    // The GPU side of the scene's environment: each map baked (ContentLoader's BakeEnvironment) and
    // uploaded the first time its path is seen and kept for the session, so switching back and forth
    // bakes nothing again; the split-sum BRDF table, computed once at start; and a black 1x1 cube,
    // the fallback for no environment or one that does not bake (which logs once, never retried).
    class EnvironmentResources
    {
    public:
        // The BRDF table's size and samples per texel (about a million samples, once).
        static constexpr uint32_t BRDF_LUT_SIZE    = 64;
        static constexpr uint32_t BRDF_LUT_SAMPLES = 256;

        explicit EnvironmentResources(RHI::IRHIDevice& device);
        ~EnvironmentResources();

        EnvironmentResources(const EnvironmentResources&)            = delete;
        EnvironmentResources& operator=(const EnvironmentResources&) = delete;

        // Makes the frame's environment the one RenderScene::Environment names, baking its map on
        // first use; outside a frame's recording, as it may upload.
        void Sync(const HX::RenderEnvironment& environment, RHI::IRHIDevice& device, const FS::FileSystemManager& fileSystem);

        // What the forward pass binds: the current environment, or the fallback.
        const ForwardEnvironment& GetForwardEnvironment() const { return m_Current; }

        size_t GetBakedCount() const { return m_Baked.size(); }

    private:
        // One map's bake; Radiance is null for a map that did not bake.
        struct BakedMap
        {
            std::unique_ptr<RHI::IRHITexture> Radiance;
            ContentLoader::ShIrradiance       Sh;
            uint32_t                          MipCount = 0;
        };

        const BakedMap& FindOrBake(const std::string& path, RHI::IRHIDevice& device, const FS::FileSystemManager& fileSystem);

        std::unordered_map<std::string, BakedMap> m_Baked;
        std::unique_ptr<RHI::IRHITexture>         m_BlackCube;
        std::unique_ptr<RHI::IRHITexture>         m_BrdfLut;
        ForwardEnvironment                        m_Current;
    };
}
