#pragma once

#include "WindowState.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include "Controls.h"

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
		void HandleInput();

		void CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface);
		GLFWwindow* GetGlfwWindow();
		Controls& GetControls();

		bool IsWindowResized() const;
		void ResetResizedState();

	public:
		static void ResizeCallback(GLFWwindow* window, int width, int height);
		static void OnKey(GLFWwindow* window, int key, int scancode, int action, int mods);
		static void OnMouseButton(GLFWwindow* window, int button, int action, int mods);
		static void OnMouseMove(GLFWwindow* window, double x, double y);
		static void OnMouseScroll(GLFWwindow* window, double x, double y);

	private:
		WindowState mWindowState;
		GLFWwindow* mWindow;

		bool mWindowResized = false;

		Controls mControls;
	};
}

