#include "Surface.h"

namespace Renderer
{
	Surface::Surface()
		: mSurface(VK_NULL_HANDLE)
	{
	}
	Surface::~Surface()
	{
		if (mSurface != nullptr)
		{
			throw std::runtime_error("Surface suold be cleanedup before destruction!");
		}
	}
	void Surface::Initialize(Instance& instance, WindowManager& windowManager)
	{
		windowManager.CreateWindowSurface(instance.GetInstance(), &mSurface);
	}

	void Surface::Cleanup(Instance& instance)
	{
		vkDestroySurfaceKHR(instance.GetInstance(), mSurface, nullptr);
		mSurface = nullptr;
	}

	VkSurfaceKHR Surface::GetSurface() const
	{
		return mSurface;
	}
}

