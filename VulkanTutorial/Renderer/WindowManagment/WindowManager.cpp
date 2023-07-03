#include "WindowManager.h"

namespace Renderer
{
	WindowManager::WindowManager()
		: mWindowState{}
		, mWindow(nullptr)
	{
	}

	WindowManager::~WindowManager()
	{
	}

	void WindowManager::Initialize(WindowState state)
	{
		InitializeThread();
		InitializeWindow(state);

		mWindowThread = std::thread(&WindowManager::MessageLoop, this);
	}

	void WindowManager::Cleanup()
	{
		glfwDestroyWindow(mWindow);
		mWindow = nullptr;
		mWindowThread.join();
		glfwTerminate();
	}

	bool WindowManager::ShouldClose()
	{
		return glfwWindowShouldClose(mWindow);
	}

	void WindowManager::CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface)
	{
		if (glfwCreateWindowSurface(instance, mWindow, nullptr, surface) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create window surface!");
		}
	}

	GLFWwindow* WindowManager::GetGlfwWindow()
	{
		return mWindow;
	}

	void WindowManager::InitializeThread()
	{
		
	}

	void WindowManager::InitializeWindow(WindowState windowState)
	{
		mWindowState = windowState;

		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
		mWindow = glfwCreateWindow(windowState.mWidth, windowState.mHeight, windowState.mWindowName.c_str(), nullptr, nullptr);
		glfwSetWindowPos(mWindow, windowState.mX, windowState.mY);
	}

	void WindowManager::MessageLoop()
	{
		while (!ShouldClose())
		{
			glfwPollEvents();
		}
	}

}

