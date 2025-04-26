#pragma once

#include <string>

namespace WinManager
{
	struct WindowState
	{
		enum class WindowMode
		{
			Windowed,
			Fullscreen
		};

		std::string windowName;

		int x;
		int y;

		int width;
		int height;

		WindowMode windowMode;

		static WindowState GetDefaultState();
	};

}

