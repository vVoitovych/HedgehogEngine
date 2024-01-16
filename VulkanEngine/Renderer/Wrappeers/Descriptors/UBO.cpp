#include "UBO.hpp"
#include "Renderer/Wrappeers/Device/Device.hpp"
#include "UBOInfo.hpp"
#include "Logger/Logger.hpp"
#include "Renderer/Common/EngineDebugBreak.hpp"
#include "Renderer/Context/RenderContext.hpp"
#include "Renderer/Context/FrameContext.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <chrono>

namespace Renderer
{
	UBO::UBO(const std::unique_ptr<Device>& device)
		: mUniformBuffer(nullptr)
		, mUniformBufferMemory(nullptr)
		, mUniformBufferMapped(nullptr)
	{
		VkDeviceSize bufferSize = sizeof(UniformBufferObject);
		device->CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			mUniformBuffer, mUniformBufferMemory);

		device->MapMemory(mUniformBufferMemory, 0, bufferSize, 0, &mUniformBufferMapped);
		LOGINFO("Vulkan UBO created");
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

	UBO::UBO(UBO&& other) noexcept
		: mUniformBuffer(other.mUniformBuffer)
		, mUniformBufferMemory(other.mUniformBufferMemory)
		, mUniformBufferMapped(other.mUniformBufferMapped)
	{
		other.mUniformBuffer = nullptr;
		other.mUniformBufferMemory = nullptr;
		other.mUniformBufferMapped = nullptr;
	}

	UBO& UBO::operator=(UBO&& other) noexcept
	{
		if (this != &other)
		{
			mUniformBuffer = other.mUniformBuffer;
			mUniformBufferMemory = other.mUniformBufferMemory;
			mUniformBufferMapped = other.mUniformBufferMapped;

			other.mUniformBuffer = nullptr;
			other.mUniformBufferMemory = nullptr;
			other.mUniformBufferMapped = nullptr;
		}
		return *this;
	}

	void UBO::Cleanup(const std::unique_ptr<Device>& device)
	{
		device->DestroyBuffer(mUniformBuffer, nullptr);
		device->FreeMemory(mUniformBufferMemory, nullptr);
		mUniformBuffer = nullptr;
		mUniformBufferMemory = nullptr;
		LOGINFO("UBO cleaned");
	}

	void UBO::UpdateUniformBuffer(std::unique_ptr< RenderContext>& context)
	{
		static auto startTime = std::chrono::high_resolution_clock::now();
		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		const auto& frameContext = context->GetFrameContext();
		UniformBufferObject ubo{};
		ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f) * 0.2f, glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.view = frameContext->GetCameraViewMatrix();
		ubo.proj = frameContext->GetCameraProjMatrix();

		memcpy(mUniformBufferMapped, &ubo, sizeof(ubo));
	}

	VkBuffer UBO::GetNativeBuffer()
	{
		return mUniformBuffer;
	}

}



