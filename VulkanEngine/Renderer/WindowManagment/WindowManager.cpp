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
		std::cout << "Window manager cleaned" << std::endl;
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

	bool WindowManager::IsWindowResized() const
	{
		return mWindowResized;
	}

	void WindowManager::ResetResizedState()
	{
		mWindowResized = false;
	}

	void WindowManager::ResizeCallback(GLFWwindow* window, int width, int height)
	{
		auto app = reinterpret_cast<WindowManager*>(glfwGetWindowUserPointer(window));
		app->mWindowResized = true;
	}

	void WindowManager::InitializeThread()
	{
		
	}

	void WindowManager::InitializeWindow(WindowState windowState)
	{
		mWindowState = windowState;

		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		
		mWindow = glfwCreateWindow(windowState.mWidth, windowState.mHeight, windowState.mWindowName.c_str(), nullptr, nullptr);
		glfwSetWindowPos(mWindow, windowState.mX, windowState.mY);
		glfwSetWindowUserPointer(mWindow, this);
		glfwSetFramebufferSizeCallback(mWindow, ResizeCallback);
	}

	void WindowManager::MessageLoop()
	{
		while (!ShouldClose())
		{
			glfwPollEvents();
		}
	}

}

