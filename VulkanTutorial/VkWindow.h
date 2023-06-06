#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>

namespace VkEngine
{
	class VkWindow
	{
	public:
		VkWindow(int inWidth, int inHeight, std::string inName);
		~VkWindow();

		VkWindow(const VkWindow& rhs) = delete;
		VkWindow& operator=(const VkWindow& rhs) = delete;

		bool shouldClose();

		void CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface);
		GLFWwindow* GetlfwWindow();
	private:
		void InitWindow();

	private:
		const int mWidth;
		const int mHeight;
		std::string mWindowName;

		GLFWwindow* mWindow;
	};

}

