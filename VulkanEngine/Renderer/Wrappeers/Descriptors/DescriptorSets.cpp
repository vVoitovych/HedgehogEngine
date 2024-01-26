#include "DescriptorSets.h"

#include "DescriptorSetLayout.hpp"
#include "UBOInfo.hpp"
#include "UBO.hpp"
#include "TextureSampler.hpp"

#include "Renderer/Wrappeers/Device/Device.hpp"
#include "Renderer/Wrappeers/Descriptors/DescriptorPool.h"
#include "Renderer/Wrappeers/Resources/TextureImage/TextureImage.hpp"

#include "Logger/Logger.hpp"
#include "Renderer/Common/EngineDebugBreak.hpp"
#include "Renderer/Common/RendererSettings.hpp"

////////////////////////////////Helper functions///////////////////////////////
inline static void CreateDescriptorBufferInfo(const VkBuffer& buffer, VkDescriptorBufferInfo& bufferInfo) 
{
    bufferInfo.buffer = buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = VK_WHOLE_SIZE;
}

inline static void CreateDescriptorImageInfo(const VkImageView& imageView, const VkSampler& sampler, VkDescriptorImageInfo& imageInfo) 
{
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = imageView;
    imageInfo.sampler = sampler;
}
///////////////////////////////////////////////////////////////////////////////


namespace Renderer
{
	DescriptorSets::DescriptorSets(
        const std::unique_ptr<Device>& logicalDevice, 
        const std::vector<DescriptorInfo>& uboInfo, 
        const std::vector<DescriptorInfo>& samplersInfo, 
        const std::vector<std::shared_ptr<TextureImage>>& textures, 
        const std::unique_ptr<DescriptorSetLayout>& descriptorSetLayout, 
        const std::unique_ptr<DescriptorPool>& descriptorPool, 
        const std::vector<UBO>& UBOs)
	{
        std::vector<VkDescriptorImageInfo> imageInfos;
        imageInfos.resize(samplersInfo.size());

        mDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        mDescriptorSetLayouts.resize(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout->GetNativeLayout());

        descriptorPool->AllocDescriptorSets(mDescriptorSetLayouts, mDescriptorSets);

        for (size_t i = 0; i < mDescriptorSets.size(); i++)
        {

            std::vector<VkDescriptorBufferInfo> bufferInfos(UBOs.size());
            for (size_t j = 0; j < UBOs.size(); j++)
            {
                
                CreateDescriptorBufferInfo(UBOs[j].GetNativeBuffer(), bufferInfos[j]);
            }

            // Samplers of textures
            for (size_t j = 0; j < textures.size(); j++)
            {
                CreateDescriptorImageInfo(
                    textures[j]->getImageView(),
                    textures[j]->getSampler(),
                    imageInfos[j]
                );
            }

            if (additionalTextures != nullptr)
            {
                createDescriptorImageInfo(
                    additionalTextures->irradianceMap->getImageView(),
                    additionalTextures->irradianceMap->getSampler(),
                    imageInfos[samplersInfo.size() - 4]
                );
                createDescriptorImageInfo(
                    additionalTextures->BRDFlut->getImageView(),
                    additionalTextures->BRDFlut->getSampler(),
                    imageInfos[samplersInfo.size() - 3]
                );
                createDescriptorImageInfo(
                    additionalTextures->prefilteredEnvMap->getImageView(),
                    additionalTextures->prefilteredEnvMap->getSampler(),
                    imageInfos[samplersInfo.size() - 2]
                );

                createDescriptorImageInfo(
                    *(additionalTextures->shadowMapView),
                    *(additionalTextures->shadowMapSampler),
                    imageInfos[samplersInfo.size() - 1]
                );
            }

            // Describes how to update the descriptors.
            // (how and which buffer/image use to bind with the each descriptor)
            std::vector<VkWriteDescriptorSet> descriptorWrites(
                uboInfo.size() + samplersInfo.size()
            );

            // UBOs
            for (size_t j = 0; j < uboInfo.size(); j++)
            {
                createDescriptorWriteInfo(
                    bufferInfos[j],
                    m_descriptorSets[i],
                    uboInfo[j].bindingNumber,
                    0,
                    uboInfo[j].descriptorType,
                    descriptorWrites[j]
                );
            }
            // Samplers
            for (size_t j = 0; j < samplersInfo.size(); j++)
            {
                createDescriptorWriteInfo(
                    imageInfos[j],
                    m_descriptorSets[i],
                    // Binding number.
                    samplersInfo[j].bindingNumber,
                    0,
                    samplersInfo[j].descriptorType,
                    descriptorWrites[uboInfo.size() + j]
                );
            }

            vkUpdateDescriptorSets(
                logicalDevice,
                static_cast<uint32_t>(descriptorWrites.size()),
                descriptorWrites.data(),
                0,
                nullptr
            );
        }
	}

	DescriptorSets::~DescriptorSets()
	{
	}

	const VkDescriptorSet& DescriptorSets::GetDescriptionSet(const size_t index) const
	{
		// TODO: insert return statement here
	}

	void DescriptorSets::Cleanup()
	{
	}

}



