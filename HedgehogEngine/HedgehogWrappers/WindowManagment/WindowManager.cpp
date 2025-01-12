#include "WindowManager.hpp"

#include "HedgehogMath/Vector.hpp"

#include "Logger/Logger.hpp"

#include "ThirdParty/tinyfiledialogs/tinyfiledialogs.h"

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

namespace WinManager
{
	std::function<bool()> WindowManager::mGuiCallback = nullptr;

	WindowManager::WindowManager()
		: mWindowState(WindowState::GetDefaultState())
		, mWindow(nullptr)
	{
		Initialize();
	}

	WindowManager::WindowManager(const WindowState& state)
		: mWindowState(state)
		, mWindow(nullptr)
	{
		Initialize();
	}

	WindowManager::~WindowManager()
	{
		glfwDestroyWindow(mWindow);
		mWindow = nullptr;
		glfwTerminate();
		LOGINFO("Window manager cleaned");
	}

	void WindowManager::Initialize()
	{
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        mWindow = glfwCreateWindow(mWindowState.mWidth, mWindowState.mHeight, mWindowState.mWindowName.c_str(), nullptr, nullptr);
        glfwSetWindowPos(mWindow, mWindowState.mX, mWindowState.mY);
        glfwSetWindowUserPointer(mWindow, this);
        glfwSetFramebufferSizeCallback(mWindow, WindowManager::ResizeCallback);
        glfwSetKeyCallback(mWindow, WindowManager::OnKey);
		glfwSetMouseButtonCallback(mWindow, WindowManager::OnMouseButton);
		glfwSetCursorPosCallback(mWindow, OnMouseMove);
		glfwSetScrollCallback(mWindow, OnMouseScroll);

		LOGINFO("Window manager initialized");
	}

	bool WindowManager::ShouldClose()
	{
		return glfwWindowShouldClose(mWindow);
	}

	void WindowManager::HandleInput()
	{
		glfwPollEvents();

	}

	void WindowManager::CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface) const
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

	const GLFWwindow* WindowManager::GetGlfwWindow() const
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

	void WindowManager::SetOnGuiCallback(std::function<bool()> func)
	{
		mGuiCallback = func;
	}

	void WindowManager::SetIcon(int width, int height, unsigned char* data)
	{
		GLFWimage images[1];
		images[0].width = width;
		images[0].height = height;
		images[0].pixels = data;

		glfwSetWindowIcon(mWindow, 1, images);
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
		(void)mods;

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

	void WindowManager::OnMouseButton(GLFWwindow* window, int button, int action, int mods)
	{
		if (mGuiCallback && mGuiCallback())
			return;
		auto app = reinterpret_cast<WindowManager*>(glfwGetWindowUserPointer(window));

		Controls& controls = app->GetControls();
		if (button == GLFW_MOUSE_BUTTON_LEFT) 
		{
			if (action == GLFW_PRESS)
			{
				if (!(controls.IsLeftMouseButton || controls.IsMiddleMouseButton || controls.IsRightMouseButton))
				{
					double x, y;
					glfwGetCursorPos(window, &x, &y);
					controls.MousePos = HM::Vector2((float)x, (float)y);
					controls.MouseDelta = HM::Vector2(0.0f, 0.0f);
				}
				controls.IsLeftMouseButton = true;
			}
			else if (action == GLFW_RELEASE)
			{
				controls.IsLeftMouseButton = false;
				controls.MouseDelta = HM::Vector2(0.0f, 0.0f);
			}
		}
		if (button == GLFW_MOUSE_BUTTON_RIGHT)
		{
			if (action == GLFW_PRESS)
			{
				if (!(controls.IsLeftMouseButton || controls.IsMiddleMouseButton || controls.IsRightMouseButton))
				{
					double x, y;
					glfwGetCursorPos(window, &x, &y);
					controls.MousePos = HM::Vector2((float)x, (float)y);
					controls.MouseDelta = HM::Vector2(0.0f, 0.0f);
				}
				controls.IsRightMouseButton = true;
			}
			else if (action == GLFW_RELEASE)
			{
				controls.IsRightMouseButton = false;
				controls.MouseDelta = HM::Vector2(0.0f, 0.0f);
			}
		}
		if (button == GLFW_MOUSE_BUTTON_MIDDLE)
		{
			if (action == GLFW_PRESS)
			{
				if (!(controls.IsLeftMouseButton || controls.IsMiddleMouseButton || controls.IsRightMouseButton))
				{
					double x, y;
					glfwGetCursorPos(window, &x, &y);
					controls.MousePos = HM::Vector2((float)x, (float)y);
					controls.MouseDelta = HM::Vector2(0.0f, 0.0f);
				}
				controls.IsMiddleMouseButton = true;
			}
			else if (action == GLFW_RELEASE)
			{
				controls.IsMiddleMouseButton = false;
				controls.MouseDelta = HM::Vector2(0.0f, 0.0f);
			}
		}
	}

	void WindowManager::OnMouseMove(GLFWwindow* window, double x, double y)
	{
		auto app = reinterpret_cast<WindowManager*>(glfwGetWindowUserPointer(window));

		Controls& controls = app->GetControls();
		if (controls.IsLeftMouseButton || controls.IsMiddleMouseButton || controls.IsRightMouseButton)
		{
			controls.MouseDelta = HM::Vector2((float)x, (float)y) - controls.MousePos;
			controls.MousePos = HM::Vector2((float)x, (float)y);

			if (abs(controls.MouseDelta.x()) < 2)
			{
				controls.MouseDelta.x() = 0.0f;
			}
			if (abs(controls.MouseDelta.y()) < 2)
			{
				controls.MouseDelta.y() = 0.0f;
			}
		}
	}

	void WindowManager::OnMouseScroll(GLFWwindow* window, double x, double y)
	{
		auto app = reinterpret_cast<WindowManager*>(glfwGetWindowUserPointer(window));

		Controls& controls = app->GetControls();
		controls.ScrollDelta = HM::Vector2((float)x, (float)y);
	}

	
}

