#include "WindowState.hpp"


namespace Renderer
{
	WindowState WindowState::GetDefaultState()
	{
		WindowState result{};
		result.mWindowName = "Vulkan Engine";
		result.mX = 100;
		result.mY = 100;
		result.mWidth = 1366;
		result.mHeight = 768;
		result.mWindowMode = WindowMode::Windowed;
		return result;
	}
}

