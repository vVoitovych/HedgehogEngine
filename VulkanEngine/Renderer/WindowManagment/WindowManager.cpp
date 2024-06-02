#include "WindowManager.hpp"
#include "Logger/Logger.hpp"
#include "Renderer/RenderPasses/GuiPass.hpp"

#include "ContentLoader/TextureLoader.hpp"

#include "ThirdParty/tinyfiledialogs/tinyfiledialogs.h"

namespace Renderer
{
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

		ContentLoader::TextureLoader texLoader;
		texLoader.LoadTexture("Textures\\Logo\\logo1.png");
		GLFWimage images[1];
		images[0].width = texLoader.GetWidth();
		images[0].height = texLoader.GetHeight();
		images[0].pixels = static_cast<unsigned char*>(texLoader.GetData());

		glfwSetWindowIcon(mWindow, 1, images);

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
		if (GuiPass::IsCursorPositionInGUI())
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
					controls.MousePos = glm::vec2((float)x, (float)y);
					controls.MouseDelta = glm::vec2(0, 0);
				}
				controls.IsLeftMouseButton = true;
			}
			else if (action == GLFW_RELEASE)
			{
				controls.IsLeftMouseButton = false;
				controls.MouseDelta = glm::vec2(0, 0);
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
					controls.MousePos = glm::vec2((float)x, (float)y);
					controls.MouseDelta = glm::vec2(0, 0);
				}
				controls.IsRightMouseButton = true;
			}
			else if (action == GLFW_RELEASE)
			{
				controls.IsRightMouseButton = false;
				controls.MouseDelta = glm::vec2(0, 0);
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
					controls.MousePos = glm::vec2((float)x, (float)y);
					controls.MouseDelta = glm::vec2(0, 0);
				}
				controls.IsMiddleMouseButton = true;
			}
			else if (action == GLFW_RELEASE)
			{
				controls.IsMiddleMouseButton = false;
				controls.MouseDelta = glm::vec2(0, 0);
			}
		}
	}

	void WindowManager::OnMouseMove(GLFWwindow* window, double x, double y)
	{
		auto app = reinterpret_cast<WindowManager*>(glfwGetWindowUserPointer(window));

		Controls& controls = app->GetControls();
		if (controls.IsLeftMouseButton || controls.IsMiddleMouseButton || controls.IsRightMouseButton)
		{
			controls.MouseDelta = glm::vec2((float)x, (float)y) - controls.MousePos;
			controls.MousePos = glm::vec2((float)x, (float)y);

			if (abs(controls.MouseDelta.x) < 2)
			{
				controls.MouseDelta.x = 0.0f;
			}
			if (abs(controls.MouseDelta.y) < 2)
			{
				controls.MouseDelta.y = 0.0f;
			}
		}
	}

	void WindowManager::OnMouseScroll(GLFWwindow* window, double x, double y)
	{
		auto app = reinterpret_cast<WindowManager*>(glfwGetWindowUserPointer(window));

		Controls& controls = app->GetControls();
		controls.ScrollDelta = glm::vec2((float)x, (float)y);
	}


}

