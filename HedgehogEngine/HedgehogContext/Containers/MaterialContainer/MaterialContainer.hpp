#pragma once

#include <string>
#include <vector>
#include <memory>

namespace Scene
{
    class Scene;
}

namespace RHI
{
    class IRHIDescriptorSetLayout;
    class IRHIDescriptorPool;
    class IRHIDescriptorSet;
    class IRHIBuffer;
}

namespace Context
{
    struct MaterialData;

    class VulkanContext;
    class TextureContainer;

    class MaterialContainer
    {
    public:
        MaterialContainer(const VulkanContext& context);
        ~MaterialContainer();

        MaterialContainer(const MaterialContainer&) = delete;
        MaterialContainer(MaterialContainer&&) = delete;
        MaterialContainer& operator=(const MaterialContainer&) = delete;
        MaterialContainer& operator=(MaterialContainer&&) = delete;

        void Update(const Scene::Scene& scene);
        void UpdateResources(const VulkanContext& VulkanContext, const TextureContainer& textureContainer);
        void SetMaterialDirty(size_t index);
        void Cleanup(const VulkanContext& context);

        void ClearMaterials();

        void CreateNewMaterial();
        void SaveMaterial(size_t index);

        void LoadBaseTexture(size_t index, const VulkanContext& context, const TextureContainer& textureContainer);

        const RHI::IRHIDescriptorSetLayout& GetRHIDescriptorSetLayout() const;
        const RHI::IRHIDescriptorSet&       GetRHIDescriptorSet(size_t index) const;

        MaterialData& GetMaterialDataByIndex(size_t index);
        const MaterialData& GetMaterialDataByIndex(size_t index) const;

    private:
        void UpdateMaterialByIndex(size_t index, const VulkanContext& context, const TextureContainer& textureContainer);
        void CreateMaterialResources(MaterialData& data, const VulkanContext& context, const TextureContainer& textureContainer);

    private:
        struct MaterialUniform
        {
            float transparency;
        };

    private:
        const std::string m_DefaultWhiteTexture = "Textures\\Default\\white.png";
        const std::string m_DefaultCellTexture = "Textures\\Default\\cells.png";

        std::vector<MaterialData> m_Materials;

        std::unique_ptr<RHI::IRHIDescriptorSetLayout>        m_RHILayout;
        std::unique_ptr<RHI::IRHIDescriptorPool>             m_RHIDescriptorPool;
        std::vector<std::unique_ptr<RHI::IRHIBuffer>>        m_RHIMaterialUniforms;
        std::vector<std::unique_ptr<RHI::IRHIDescriptorSet>> m_RHIDescriptorSets;
    };

}
