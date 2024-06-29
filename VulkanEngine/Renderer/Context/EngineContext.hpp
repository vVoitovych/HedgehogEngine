#pragma once

#include <vulkan/vulkan.h>
#include <memory>

namespace Scene
{
	class Scene;
}

namespace Renderer
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

		std::unique_ptr<Camera> mCamera;
		std::unique_ptr<Scene::Scene> mScene;

		std::unique_ptr<MeshContainer> mMeshContainer;
		std::unique_ptr<TextureContainer> mTextureContainer;
		std::unique_ptr<LightContainer> mLightContainer;
		std::unique_ptr<MaterialContainer> mMaterialContainer;
		std::unique_ptr< DrawListContainer> mDrawListContainer;
	};

}




