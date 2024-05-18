#pragma once

#include "Renderer/Camera/Camera.hpp"
#include "Scene/Scene.hpp"
#include "Renderer/Containers/MeshContainer.hpp"
#include "Renderer/Containers/TextureContainer.hpp"
#include "Renderer/Containers/LightContainer.hpp"

#include "Renderer/WindowManagment/WindowManager.hpp"

#include <vulkan/vulkan.h>
#include <memory>

namespace Renderer
{
	class VulkanContext;

	class EngineContext
	{
	public:
		EngineContext(const VulkanContext& vulkanContext);
		void Cleanup(const VulkanContext& vulkanContext);

		void UpdateContext(VulkanContext& vulkanContext, float dt);

		const MeshContainer& GetMeshContainer() const;
		const TextureContaineer& GetTextureContainer() const;
		const LightContainer& GetLightContainer() const;

		const Camera& GetCamera() const;
		Scene::Scene& GetScene();
		const Scene::Scene& GetScene() const;
	private:

		Camera mCamera;

		Scene::Scene mScene;

		MeshContainer mMeshContainer;
		TextureContaineer mTextureContainer;
		LightContainer mLightContainer;
	};

}




