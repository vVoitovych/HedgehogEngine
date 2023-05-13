#include "Application.h"

namespace VkEngine
{
	void VkApplication::Run()
	{
		while (!mWindow.shouldClose())
		{
			glfwPollEvents();
		}
	}
}

