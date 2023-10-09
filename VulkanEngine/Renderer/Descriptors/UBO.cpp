#include "UBO.h"
#include "VulkanEngine/Renderer/Device/Device.h"
#include "UBOInfo.h"
#include "VulkanEngine/Renderer/Common/CommonFunctions.h"
#include "VulkanEngine/Logger/Logger.h"
#include "VulkanEngine/Renderer/Common/EngineDebugBreak.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Renderer
{
	UBO::UBO()
		: mDevice(nullptr)
		, mUniformBuffer(nullptr)
		, mUniformBufferMemory(nullptr)
		, mUniformBufferMapped(nullptr)
	{
	}
	UBO::~UBO()
	{
		if (mUniformBuffer != nullptr)
		{
			LOGERROR("Vulkan uniform buffer should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
		if (mUniformBufferMemory != nullptr)
		{
			LOGERROR("Vulkan uniform buffer memory should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}

	void UBO::Initialize(Device& device)
	{
		mDevice = device.GetNativeDevice();
		VkDeviceSize bufferSize = sizeof(UniformBufferObject);
		CreateBuffer(mDevice,
			device.GetNativePhysicalDevice(), 
			bufferSize, 
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
			mUniformBuffer, mUniformBufferMemory);

		vkMapMemory(mDevice, mUniformBufferMemory, 0, bufferSize, 0, &mUniformBufferMapped);
		LOGINFO("Vulkan UBO created");
	}

	void UBO::Cleanup()
	{
		vkDestroyBuffer(mDevice, mUniformBuffer, nullptr);
		vkFreeMemory(mDevice, mUniformBufferMemory, nullptr);
		mUniformBuffer = nullptr;
		mUniformBufferMemory = nullptr;
		LOGINFO("UBO cleaned");
	}

	void UBO::UpdateUniformBuffer(float time, float ratio)
	{
		UniformBufferObject ubo{};
		ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.proj = glm::perspective(glm::radians(45.0f), ratio, 0.1f, 10.0f);
		ubo.proj[1][1] *= -1;

		memcpy(mUniformBufferMapped, &ubo, sizeof(ubo));
	}
	VkBuffer UBO::GetNativeBuffer()
	{
		return mUniformBuffer;
	}
}



