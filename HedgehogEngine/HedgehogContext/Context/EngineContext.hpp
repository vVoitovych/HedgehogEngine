#pragma once

#include <vulkan/vulkan.h>
#include <memory>

namespace Scene
{
	class Scene;
}

namespace HedgehogSettings
{
	class Settings;
}

namespace Context
{
	class VulkanContext;
	class Camera;
	class MeshContainer;
	class TextureContainer;
	class LightContainer;
	class MaterialContainer;
	class DrawListContainer;

	class EngineContext
	{
	public:
		EngineContext(const VulkanContext& vulkanContext);
		~EngineContext();
		void Cleanup(const VulkanContext& vulkanContext);

		void UpdateContext(VulkanContext& vulkanContext, float dt);

		const MeshContainer& GetMeshContainer() const;
		const TextureContainer& GetTextureContainer() const;
		const LightContainer& GetLightContainer() const;
		const MaterialContainer& GetMaterialContainer() const;
		MaterialContainer& GetMaterialContainer();
		const DrawListContainer& GetDrawListContainer() const;

		const Camera& GetCamera() const;
		Scene::Scene& GetScene();
		const Scene::Scene& GetScene() const;
	private:

		std::unique_ptr<Camera> m_Camera;
		std::unique_ptr<Scene::Scene> m_Scene;

		std::unique_ptr<MeshContainer> m_MeshContainer;
		std::unique_ptr<TextureContainer> m_TextureContainer;
		std::unique_ptr<LightContainer> m_LightContainer;
		std::unique_ptr<MaterialContainer> m_MaterialContainer;
		std::unique_ptr<DrawListContainer> m_DrawListContainer;

		std::unique_ptr<HedgehogSettings::Settings> m_Settings;
	};

}




