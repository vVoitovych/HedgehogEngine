#include "LogColorized.hpp"
#include <Windows.h>

namespace Hedgehog
{
	namespace Logger
	{
		LogColorized::LogColorized()
		{
			mConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		}

		void LogColorized::SetLogColor(uint16_t color)
		{
			SetConsoleTextAttribute(mConsole, color);
		}
	}
}

