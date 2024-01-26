#pragma once

#include "Renderer/Camera/Camera.hpp"
#include "Scene/Scene.hpp"
#include "Renderer/Containers/MeshContainer.hpp"
#include "Renderer/Containers/TextureContainer.hpp"
#include "Renderer/Containers/SamplerContainer.h"

#include "Renderer/WindowManagment/WindowManager.hpp"

#include <vulkan/vulkan.h>
#include <memory>

namespace Renderer
{
	class VulkanContext;

	class EngineContext
	{
	public:
		EngineContext(const std::unique_ptr<VulkanContext>& vulkanContext);
		void Cleanup(const std::unique_ptr<VulkanContext>& vulkanContext);

		void UpdateContext(const std::unique_ptr<VulkanContext>& vulkanContext, float dt);

		const MeshContainer& GetMeshContainer() const;
		const TextureContaineer& GetTextureContainer() const;
		const SamplerContainer& GetSamplerContainer() const;

		const Camera& GetCamera() const;

	private:

		Camera mCamera;

		Scene::Scene mScene;

		MeshContainer mMeshContainer;
		TextureContaineer mTextureContainer;
		SamplerContainer mSamplerContainer;

	};

}




