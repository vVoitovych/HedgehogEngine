#pragma once

#include <string>

namespace WinManager
{
	class WindowState
	{
	public:
		enum class WindowMode
		{
			Windowed,
			Fullscreen
		};

	public:
		std::string mWindowName;

		int mX;
		int mY;

		int mWidth;
		int mHeight;

		WindowMode mWindowMode;

	public:
		static WindowState GetDefaultState();
	};

}

