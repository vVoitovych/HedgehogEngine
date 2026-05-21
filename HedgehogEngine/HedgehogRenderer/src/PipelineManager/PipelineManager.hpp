#pragma once

#include "RHI/api/IRHIPipeline.hpp"
#include "RHI/api/RHITypes.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace RHI
{
    class IRHIDevice;
}

namespace Renderer
{
    class ShaderManager;

    class PipelineManager
    {
    public:
        PipelineManager(RHI::IRHIDevice& device, ShaderManager& shaderManager);
        void Cleanup();

        // Returns a cached pipeline or creates a new one. The reference is valid until
        // the entry is evicted by InvalidateShadersOf() or Cleanup() is called.
        RHI::IRHIPipeline& GetOrCreate(const RHI::GraphicsPipelineDesc& desc);

        // Remove all cached pipelines whose descriptor references the given SPIR-V path.
        // Intended for Phase 10 hot-reload; safe to call at any time.
        void InvalidateShadersOf(const std::string& spirvPath);

        // Build DescriptorPool sizes from a single set's bindings.
        static std::vector<RHI::PoolSize> MakePoolSizes(
            const std::vector<RHI::DescriptorBinding>& bindings,
            uint32_t                                   maxSets);

    private:
        static size_t HashDesc(const RHI::GraphicsPipelineDesc& desc);

        struct CacheEntry
        {
            RHI::GraphicsPipelineDesc          desc;
            std::unique_ptr<RHI::IRHIPipeline> pipeline;
        };

        RHI::IRHIDevice&                       m_Device;
        ShaderManager&                         m_ShaderManager;
        std::unordered_map<size_t, CacheEntry> m_Cache;
    };
}
