#pragma once
#include <stdint.h>

namespace Hedgehog
{
	namespace Logger
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
}




