#pragma once

#include "MaterialGpuData.hpp"

#include "RHI/api/RHITypes.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace HedgehogEngine
{
    class MaterialContainer;
    class TextureContainer;
}

namespace RHI
{
    class IRHIDevice;
    class IRHITexture;
    class IRHISampler;
    class IRHIDescriptorSetLayout;
    class IRHIDescriptorPool;
    class IRHIDescriptorSet;
}

namespace Renderer
{
    class MaterialSync
    {
    public:
        MaterialSync()  = default;
        ~MaterialSync() = default;

        MaterialSync(const MaterialSync&)            = delete;
        MaterialSync& operator=(const MaterialSync&) = delete;
        MaterialSync(MaterialSync&&)                 = delete;
        MaterialSync& operator=(MaterialSync&&)      = delete;

        void SetMaterialLayout(RHI::IRHIDevice&                    device,
                               const RHI::IRHIDescriptorSetLayout& layout,
                               uint32_t                            maxSets,
                               const std::vector<RHI::PoolSize>&   poolSizes);

        void Sync(HedgehogEngine::MaterialContainer& container,
                  HedgehogEngine::TextureContainer&  texContainer,
                  RHI::IRHIDevice&                   device);

        const RHI::IRHIDescriptorSet& GetMaterialDescriptorSet(uint32_t index) const;

        void Cleanup(RHI::IRHIDevice& device);

    private:
        RHI::IRHITexture& GetOrCreateTexture(const std::string& path, RHI::IRHIDevice& device);

        void CreateMaterialGpu(float transparency, const std::string& texturePath, RHI::IRHIDevice& device);
        void UpdateMaterialGpu(uint32_t index, float transparency, const std::string& texturePath,
                               RHI::IRHIDevice& device);

    private:
        struct MaterialUniform
        {
            float m_Transparency;
        };

    private:
        const RHI::IRHIDescriptorSetLayout*      m_MaterialLayout = nullptr;
        std::unique_ptr<RHI::IRHIDescriptorPool> m_MaterialPool;
        std::vector<MaterialGpuData>             m_Materials;
        size_t                                   m_RegisteredMaterialCount = 0;

        std::unordered_map<std::string, std::unique_ptr<RHI::IRHITexture>> m_TextureCache;
        std::unique_ptr<RHI::IRHISampler>                                  m_LinearSampler;
    };
}
