#include "LogColorized.h"
#include <Windows.h>

namespace EngineLogger
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

