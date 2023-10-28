#include "WindowManager.h"
#include "VulkanEngine/Logger/Logger.h"

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
        mWindowState = state;

        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        mWindow = glfwCreateWindow(state.mWidth, state.mHeight, state.mWindowName.c_str(), nullptr, nullptr);
        glfwSetWindowPos(mWindow, state.mX, state.mY);
        glfwSetWindowUserPointer(mWindow, this);
        glfwSetFramebufferSizeCallback(mWindow, WindowManager::ResizeCallback);
        glfwSetKeyCallback(mWindow, WindowManager::OnKey);
	}

	void WindowManager::Cleanup()
	{
		glfwDestroyWindow(mWindow);
		mWindow = nullptr;
		glfwTerminate();
		LOGINFO("Window manager cleaned");
	}

	bool WindowManager::ShouldClose()
	{
		return glfwWindowShouldClose(mWindow);
	}

	void WindowManager::HandleInput()
	{
		glfwPollEvents();

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

	Controls& WindowManager::GetControls()
	{
		return mControls;
	}

	void WindowManager::OnKey(GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		auto app = reinterpret_cast<WindowManager*>(glfwGetWindowUserPointer(window));

		const bool press_or_repeat = (action == GLFW_PRESS || action == GLFW_REPEAT);

		(void)mods; // Modifiers are not reliable across system

		Controls& controls = app->GetControls();

		controls.IsPressedControl = mods & GLFW_MOD_CONTROL;

        switch (key)
        {
        case GLFW_KEY_W:
            controls.IsPressedW = press_or_repeat;
            break;
        case GLFW_KEY_S:
            controls.IsPressedS = press_or_repeat;
            break;
        case GLFW_KEY_A:
            controls.IsPressedA = press_or_repeat;
            break;
        case GLFW_KEY_D:
            controls.IsPressedD = press_or_repeat;
            break;
        case GLFW_KEY_Q:
            controls.IsPressedQ = press_or_repeat;
            break;
        case GLFW_KEY_E:
            controls.IsPressedE = press_or_repeat;
            break;       
        default:
            break;
        }
	}


}

