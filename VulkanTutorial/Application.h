#pragma once

#include "VkWindow.h"

namespace VkEngine
{
	class VkApplication
	{
	public:


		void Run();
	public:
		static constexpr int sWindowWidth = 800;
		static constexpr int sWindowHeight = 600;

	private:
		VkWindow mWindow{ sWindowWidth, sWindowHeight, "Vulkan App"};

	};
}



