#pragma once

#include "Renderer/Camera/Camera.hpp"
#include "Scene/Scene.hpp"
#include "Renderer/Containers/MeshContainer.hpp"

#include "Renderer/WindowManagment/WindowManager.hpp"

#include <vulkan/vulkan.h>
#include <memory>

namespace Renderer
{
	class Device;
	class SwapChain;

	class EngineContext
	{
	public:
		EngineContext(const std::unique_ptr<Device>& device, const std::unique_ptr<SwapChain>& swapChain, std::unique_ptr<WindowManager>&& windowManager);
		void Cleanup(const std::unique_ptr<Device>& device);

		void HandleInput();
		void UpdateContext(float dt);

		void UpdateBackBufferIdex(uint32_t index) const;
		uint32_t GetBackBufferIndex() const;
		VkExtent2D GetExtent();

		MeshContainer& GetMeshContainer();
		const std::unique_ptr<WindowManager>& GetWindowManager() const;

		bool ShouldClose() const;
		void ResizeWindow();
		bool IsWindowResized();
		void ResetWindowResizeState(VkExtent2D extent);

		const Camera& GetCamera() const;

	private:
		std::unique_ptr<WindowManager> mWindowManager;

		Camera mCamera;
		Scene::Scene mScene;
		VkExtent2D mExtent;
		MeshContainer mMeshContainer;

		mutable uint32_t mBackBufferIndex;
		bool mWindowResized = false;

	};

}




