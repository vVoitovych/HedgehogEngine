#include "WindowManager.hpp"

#include "HedgehogMath/Vector.hpp"

#include "Logger/Logger.hpp"

#include "ThirdParty/tinyfiledialogs/tinyfiledialogs.h"

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

namespace WinManager
{
	std::function<bool()> WindowManager::m_GuiCallback = nullptr;

	WindowManager::WindowManager()
		: m_WindowState(WindowState::GetDefaultState())
		, m_Window(nullptr)
	{
		Initialize();
	}

	WindowManager::WindowManager(const WindowState& state)
		: m_WindowState(state)
		, m_Window(nullptr)
	{
		Initialize();
	}

	WindowManager::~WindowManager()
	{
		glfwDestroyWindow(m_Window);
		m_Window = nullptr;
		glfwTerminate();
		LOGINFO("Window manager cleaned");
	}

	void WindowManager::Initialize()
	{
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        m_Window = glfwCreateWindow(m_WindowState.width, m_WindowState.height, m_WindowState.windowName.c_str(), nullptr, nullptr);
        glfwSetWindowPos(m_Window, m_WindowState.x, m_WindowState.y);
        glfwSetWindowUserPointer(m_Window, this);
        glfwSetFramebufferSizeCallback(m_Window, WindowManager::ResizeCallback);
        glfwSetKeyCallback(m_Window, WindowManager::OnKey);
		glfwSetMouseButtonCallback(m_Window, WindowManager::OnMouseButton);
		glfwSetCursorPosCallback(m_Window, OnMouseMove);
		glfwSetScrollCallback(m_Window, OnMouseScroll);

		LOGINFO("Window manager initialized");
	}

	bool WindowManager::ShouldClose()
	{
		return glfwWindowShouldClose(m_Window);
	}

	void WindowManager::HandleInput()
	{
		glfwPollEvents();

	}

	void WindowManager::CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface) const
	{
		if (glfwCreateWindowSurface(instance, m_Window, nullptr, surface) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create window surface!");
		}
	}

	GLFWwindow* WindowManager::GetGlfwWindow() 
	{
		return m_Window;
	}

	const GLFWwindow* WindowManager::GetGlfwWindow() const
	{
		return m_Window;
	}

	bool WindowManager::IsWindowResized() const
	{
		return m_WindowResized;
	}

	void WindowManager::ResetResizedState()
	{
		m_WindowResized = false;
	}

	void WindowManager::SetOnGuiCallback(std::function<bool()> func)
	{
		m_GuiCallback = func;
	}

	void WindowManager::SetIcon(int width, int height, unsigned char* data)
	{
		GLFWimage images[1];
		images[0].width = width;
		images[0].height = height;
		images[0].pixels = data;

		glfwSetWindowIcon(m_Window, 1, images);
	}

	void WindowManager::ResizeCallback(GLFWwindow* window, int width, int height)
	{
		auto app = reinterpret_cast<WindowManager*>(glfwGetWindowUserPointer(window));
		app->m_WindowResized = true;
	}

	Controls& WindowManager::GetControls() 
	{
		return m_Controls;
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
		if (m_GuiCallback && m_GuiCallback())
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

