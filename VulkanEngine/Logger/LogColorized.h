#pragma once
#include <stdint.h>

namespace EngineLogger
{
	class LogColorized
	{
	public:
		LogColorized();

		void SetLogColor(uint16_t color);
	private:
		void* mConsole;
	};
}





