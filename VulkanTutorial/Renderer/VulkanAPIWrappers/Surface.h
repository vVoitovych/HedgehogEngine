#pragma once

#include "../Common/pch.h"

#include "Instance.h"
#include "../WindowManagment/WindowManager.h"

namespace Renderer
{
	class Surface
	{
	public:
		Surface();
		~Surface();

		Surface(const Surface&) = delete;
		Surface& operator=(const Surface&) = delete;

		void Initialize(Instance& instance, WindowManager& windowManager);
		void Cleanup(Instance& instance);

		VkSurfaceKHR GetSurface() const;

	private:
		VkSurfaceKHR mSurface;
	};


}

