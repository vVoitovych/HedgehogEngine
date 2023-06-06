#include "VkWindow.h"

// std lib
#include <stdexcept>


namespace VkEngine
{
	VkWindow::VkWindow(int inWidth, int inHeight, std::string inName)
		: mWidth(inWidth)
		, mHeight(inHeight)
		, mWindowName(inName)
		, mWindow(nullptr)
	{
		InitWindow();
	}

	VkWindow::~VkWindow()
	{
		glfwDestroyWindow(mWindow);

		glfwTerminate();
	}

	void VkWindow::InitWindow()
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
		mWindow = glfwCreateWindow(mWidth, mHeight, mWindowName.c_str(), nullptr, nullptr);

	}

	bool VkWindow::shouldClose()
	{
		return glfwWindowShouldClose(mWindow);
	}

	void VkWindow::CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface)
	{
		if (glfwCreateWindowSurface(instance, mWindow, nullptr, surface) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create window surface!");
		}

	}

	GLFWwindow* VkWindow::GetlfwWindow()
	{
		return mWindow;
	}


}
