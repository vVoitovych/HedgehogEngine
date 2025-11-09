#pragma once

#include <vulkan/vulkan.h>

namespace Wrappers
{
	class Instance;

	class DebugMessager
	{
	public:
		DebugMessager(const Instance& instance);
		~DebugMessager();

		DebugMessager(const DebugMessager&) = delete;
		DebugMessager(DebugMessager&&) = delete;
		DebugMessager& operator=(const DebugMessager&) = delete;
		DebugMessager& operator=(DebugMessager&&) = delete;

		void Cleanup(const Instance& instance);
	private:
		VkDebugUtilsMessengerEXT m_DebugMessenger;
	};


}


