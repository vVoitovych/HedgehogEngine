#include "DebugMessanger.hpp"
#include "Instance.hpp"
#include "DebugCallback.hpp"

#include <cassert>

namespace Wrappers
{
	DebugMessager::DebugMessager(const Instance& instance)
	{
#ifdef DEBUG
		VkDebugUtilsMessengerCreateInfoEXT createInfo = instance.GetDebugMessangerInfo();
		assert(CreateDebugUtilsMessengerEXT(instance.GetNativeInstance(), &createInfo, nullptr, &m_DebugMessenger) == VK_SUCCESS);
#endif
	}

	DebugMessager::~DebugMessager()
	{
		assert(m_DebugMessenger != nullptr);
	}

	void DebugMessager::Cleanup(const Instance& instance)
	{
#ifdef DEBUG
		DestroyDebugUtilsMessengerEXT(instance.GetNativeInstance(), m_DebugMessenger, nullptr);
#endif
	}

}

