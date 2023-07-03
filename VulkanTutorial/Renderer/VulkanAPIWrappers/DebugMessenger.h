#pragma once

#include "../Common/pch.h"
#include "Instance.h"

namespace Renderer
{
	class DebugMessenger
	{
	public:
		DebugMessenger();
		~DebugMessenger();

		DebugMessenger(const DebugMessenger&) = delete;
		DebugMessenger& operator=(const DebugMessenger&) = delete;

		void Initialize(Instance& instance);
		void Cleanup(Instance& instance);
	private:
		VkDebugUtilsMessengerEXT mDebugMessenger;
	};
}

