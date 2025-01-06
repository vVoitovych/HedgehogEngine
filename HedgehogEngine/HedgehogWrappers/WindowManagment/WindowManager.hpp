#pragma once

#include "Controls.hpp"
#include "WindowState.hpp"

#include <vulkan/vulkan.h>
#include <functional>

struct GLFWwindow;

namespace WinManager
{
	class WindowManager
	{
	public:
		WindowManager();
		WindowManager(const WindowState& state);
		~WindowManager();

		WindowManager(const WindowManager& rhs) = delete;
		WindowManager(WindowManager&& rhs) = default;
		WindowManager& operator=(const WindowManager& rhs) = delete;
		WindowManager& operator=(WindowManager&& rhs) = default;

		bool ShouldClose();
		void HandleInput();

		void CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface) const;
		GLFWwindow* GetGlfwWindow();
		const GLFWwindow* GetGlfwWindow() const;
		Controls& GetControls();

		bool IsWindowResized() const;
		void ResetResizedState();

		static void SetOnGuiCallback(std::function<bool()> func);

	public:
		static void ResizeCallback(GLFWwindow* window, int width, int height);
		static void OnKey(GLFWwindow* window, int key, int scancode, int action, int mods);
		static void OnMouseButton(GLFWwindow* window, int button, int action, int mods);
		static void OnMouseMove(GLFWwindow* window, double x, double y);
		static void OnMouseScroll(GLFWwindow* window, double x, double y);

	private:
		void Initialize();

	private:
		WindowState mWindowState;
		GLFWwindow* mWindow;

		bool mWindowResized = false;

		Controls mControls;

		static std::function<bool()> mGuiCallback;
	};
}

