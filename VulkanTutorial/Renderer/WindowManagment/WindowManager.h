#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "../Common/pch.h"

#include "WindowState.h"

namespace Renderer
{
	class WindowManager
	{
	public:
		WindowManager();
		~WindowManager();

		void Initialize(WindowState state);
		void Cleanup();

		WindowManager(const WindowManager& rhs) = delete;
		WindowManager& operator=(const WindowManager& rhs) = delete;

		bool ShouldClose();

		void CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface);
		GLFWwindow* GetGlfwWindow();

	private:
		void InitializeThread();
		void InitializeWindow(WindowState windowState);
		void MessageLoop();
	private:
		WindowState mWindowState;
		GLFWwindow* mWindow;

		std::thread mWindowThread;
	};
}

