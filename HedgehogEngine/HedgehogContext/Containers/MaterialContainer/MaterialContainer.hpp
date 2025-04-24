#pragma once

#include <string>
#include <vector>
#include <memory>

namespace Scene
{
	class Scene;
}

namespace Wrappers
{
	class DescriptorSetLayout;
	class DescriptorAllocator;
	class DescriptorSet;
	class Buffer;
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

		[[deprecated]]
		void ClearMaterials();

		[[deprecated]]
		void CreateNewMaterial();
		[[deprecated]]
		void SaveMaterial(size_t index);
		[[deprecated]]
		void LoadBaseTexture(size_t index, const VulkanContext& context, const TextureContainer& textureContainer);

		[[deprecated]]
		const Wrappers::DescriptorSetLayout& GetDescriptorSetLayout() const;
		[[deprecated]]
		const Wrappers::DescriptorSet& GetDescriptorSet(size_t index) const;
		[[deprecated]]
		Wrappers::DescriptorSet& GetDescriptorSet(size_t index);
		[[deprecated]]
		MaterialData& GetMaterialDataByIndex(size_t index);
		[[deprecated]]
		const MaterialData& GetMaterialDataByIndex(size_t index) const;
	private:
		[[deprecated]]
		void UpdateMaterialByIndex(size_t index, const VulkanContext& context, const TextureContainer& textureContainer);
		[[deprecated]]
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

		std::unique_ptr<Wrappers::DescriptorSetLayout> mLayout;
		std::unique_ptr<Wrappers::DescriptorAllocator> mDescriptorAllocator;
		std::vector<Wrappers::Buffer> mMaterialUniforms;
		std::vector<Wrappers::DescriptorSet> mDescriptorSets;

	};

}



