#pragma once

#include <vulkan/vulkan.h>

namespace Renderer
{
	class Device;
	class DescriptorSetLayout;
	class UBO;
	class TextureImage;
	class TextureSampler;

	class DescriptorSet
	{
	public:
		DescriptorSet();
		~DescriptorSet();

		DescriptorSet(const DescriptorSet&) = delete;
		DescriptorSet& operator=(const DescriptorSet&) = delete;

		void Initialize(const Device& device, DescriptorSetLayout& descriptorSetLayout, UBO& ubo, TextureImage& image, TextureSampler& sampler);
		void Cleanup(const Device& device);

		VkDescriptorSet* GetNativeSet();
	private:
		VkDescriptorSet mDescriptorSet;

	};


}






