#include "UBO.hpp"
#include "Renderer/Wrappeers/Device/Device.hpp"
#include "Renderer/Wrappeers/Resources/Buffer/Buffer.hpp"
#include "Renderer/Containers/LightContainer.hpp"
#include "UBOInfo.hpp"
#include "Logger/Logger.hpp"
#include "Renderer/Common/EngineDebugBreak.hpp"
#include "Renderer/Context/RenderContext.hpp"
#include "Renderer/Context/EngineContext.hpp"
#include "Renderer/Context/FrameContext.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <chrono>

namespace Renderer
{
	UBO::UBO(const std::unique_ptr<Device>& device)
		: mUniformBufferMapped(nullptr)
	{
		VkDeviceSize bufferSize = sizeof(UniformBufferObject);
		mUniformBuffer = std::make_unique<Buffer>(device, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		mUniformBuffer->MapMemory(0, bufferSize, 0, &mUniformBufferMapped);
		LOGINFO("Vulkan UBO created");
	}

	UBO::~UBO()
	{
	}

	UBO::UBO(UBO&& other) noexcept
		: mUniformBuffer(std::move(other.mUniformBuffer))
		, mUniformBufferMapped(other.mUniformBufferMapped)
	{
		other.mUniformBufferMapped = nullptr;
	}

	UBO& UBO::operator=(UBO&& other) noexcept
	{
		if (this != &other)
		{
			mUniformBuffer = std::move(other.mUniformBuffer);
			mUniformBufferMapped = other.mUniformBufferMapped;

			other.mUniformBufferMapped = nullptr;
		}
		return *this;
	}

	void UBO::Cleanup()
	{
		mUniformBuffer->DestroyBuffer();
		LOGINFO("UBO cleaned");
	}

	void UBO::UpdateUniformBuffer(std::unique_ptr<RenderContext>& context)
	{
		const auto& frameContext = context->GetFrameContext();
		const auto& engineContext = context->GetEngineContext();
		const auto& lightContainer = engineContext->GetLightContainer();
		UniformBufferObject ubo{};
		ubo.view = frameContext->GetCameraViewMatrix();
		ubo.proj = frameContext->GetCameraProjMatrix();
		ubo.eyePosition = glm::vec4(frameContext->GetCameraPosition(), 1.0f);
		ubo.lightCount = lightContainer.GetLightCount();
		const auto& lights = lightContainer.GetLights();
		for (size_t i = 0; i < ubo.lightCount; ++i)
		{
			ubo.lights[i] = lights[i];
		}
		memcpy(mUniformBufferMapped, &ubo, sizeof(ubo));
	}

	VkBuffer UBO::GetNativeBuffer()
	{
		return mUniformBuffer->GetNativeBuffer();
	}

	const std::unique_ptr<Buffer>& UBO::GetBuffer() const
	{
		return mUniformBuffer;
	}

}



