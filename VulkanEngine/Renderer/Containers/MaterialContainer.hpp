#pragma once

#include <string>
#include <vector>
#include <memory>

namespace Scene
{
	class Scene;
}

namespace Renderer
{
	class MaterialData;

	class DescriptorSetLayout;
	class DescriptorAllocator;
	class DescriptorSet;

	class VulkanContext;
	class TextureContainer;
	class Buffer;

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

		const DescriptorSetLayout& GetDescriptorSetLayout() const;
		const DescriptorSet& GetDescriptorSet(size_t index) const;
		DescriptorSet& GetDescriptorSet(size_t index);

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
		const std::string mDefaultWtiteTexture = "Textures\\Default\\white.png";
		const std::string mDefaultCellTexture = "Textures\\Default\\cells.png";

		std::vector<MaterialData> mMaterials;

		std::unique_ptr<DescriptorSetLayout> mLayout;
		std::unique_ptr<DescriptorAllocator> mDescriptorAllocator;
		std::vector<Buffer> mMaterialUniforms;
		std::vector<DescriptorSet> mDescriptorSets;

	};

}



