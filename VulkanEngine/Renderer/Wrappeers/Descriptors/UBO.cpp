#include "UBO.hpp"
#include "VulkanEngine/Renderer/Wrappeers/Device/Device.hpp"
#include "UBOInfo.hpp"
#include "VulkanEngine/Logger/Logger.hpp"
#include "VulkanEngine/Renderer/Common/EngineDebugBreak.hpp"
#include "VulkanEngine/Renderer/Camera/Camera.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace Renderer
{
	UBO::UBO()
		: mUniformBuffer(nullptr)
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

	void UBO::Initialize(const Device& device)
	{
		VkDeviceSize bufferSize = sizeof(UniformBufferObject);
		device.CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
			mUniformBuffer, mUniformBufferMemory);

		device.MapMemory(mUniformBufferMemory, 0, bufferSize, 0, &mUniformBufferMapped);
		LOGINFO("Vulkan UBO created");
	}

	void UBO::Cleanup(const Device& device)
	{
		device.DestroyBuffer(mUniformBuffer, nullptr);
		device.FreeMemory(mUniformBufferMemory, nullptr);
		mUniformBuffer = nullptr;
		mUniformBufferMemory = nullptr;
		LOGINFO("UBO cleaned");
	}

	void UBO::UpdateUniformBuffer(float time, Camera& camera)
	{
		UniformBufferObject ubo{};
		ubo.model = glm::rotate(glm::mat4(1.0f), 0.0f * time * glm::radians(90.0f) * 0.2f, glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.view = camera.GetViewMatrix();
		ubo.proj = camera.GetProjectionMatrix();

		memcpy(mUniformBufferMapped, &ubo, sizeof(ubo));
	}

	VkBuffer UBO::GetNativeBuffer()
	{
		return mUniformBuffer;
	}

}



