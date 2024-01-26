#pragma once

#include "DescriptorInfo.h"

#include <vulkan/vulkan.h>
#include <memory>
#include <vector>

namespace Renderer
{
	class Device;
	class DescriptorPool;
	class DescriptorSetLayout;
	class UBO;
	class TextureImage;
	class TextureSampler;

	class DescriptorSets
	{
	public:
		DescriptorSets(
			const std::unique_ptr<Device>& logicalDevice,
			const std::vector<DescriptorInfo>& uboInfo,
			const std::vector<DescriptorInfo>& samplersInfo,
			const std::vector<std::shared_ptr<TextureImage>>& textures,
			const std::unique_ptr<DescriptorSetLayout>& descriptorSetLayout,
			const std::unique_ptr<DescriptorPool>& descriptorPool,
			const std::vector<UBO>& UBOs
		);
		~DescriptorSets();

		DescriptorSets(const DescriptorSets&) = delete;
		DescriptorSets& operator=(const DescriptorSets&) = delete;

		const VkDescriptorSet& GetDescriptionSet(const size_t index) const;

		void Cleanup();

	private:
		std::vector<VkDescriptorSetLayout> mDescriptorSetLayouts;
		std::vector<VkDescriptorSet> mDescriptorSets;

	};
}



