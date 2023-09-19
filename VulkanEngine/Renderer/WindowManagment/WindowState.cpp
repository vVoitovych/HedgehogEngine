#include "WindowState.h"


namespace Renderer
{
	WindowState WindowState::GetDefaultState()
	{
		WindowState result{};
		result.mWindowName = "Vulkan Engine";
		result.mX = 100;
		result.mY = 100;
		result.mWidth = 800;
		result.mHeight = 600;
		result.mWindowMode = WindowMode::Windowed;
		return result;
	}
}

