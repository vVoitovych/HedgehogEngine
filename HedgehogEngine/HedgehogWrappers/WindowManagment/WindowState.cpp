#include "WindowState.hpp"


namespace WinManager
{
	WindowState WindowState::GetDefaultState()
	{
		WindowState result{};
		result.windowName = "Hengehog Engine";
		result.x = 100;
		result.y = 100;
		result.width = 1366;
		result.height = 768;
		result.windowMode = WindowMode::Windowed;
		return result;
	}
}

