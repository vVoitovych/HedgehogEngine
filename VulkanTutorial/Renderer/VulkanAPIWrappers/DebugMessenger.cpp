#include "DebugMessenger.h"

namespace Renderer
{
	VkResult CreateDebugUtilsMessengerEXT(
		VkInstance instance,
		const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkDebugUtilsMessengerEXT* pDebugMessenger)
	{
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr)
		{
			return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
		}
		else
		{
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}

	void DestroyDebugUtilsMessengerEXT(
		VkInstance instance,
		VkDebugUtilsMessengerEXT debugMessenger,
		const VkAllocationCallbacks* pAllocator)
	{
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr)
		{
			func(instance, debugMessenger, pAllocator);
		}
	}


	DebugMessenger::DebugMessenger()
		: mDebugMessenger(VK_NULL_HANDLE)
	{
	}

	DebugMessenger::~DebugMessenger()
	{
		if (mDebugMessenger != nullptr)
		{
			throw std::runtime_error("Debug messenger shoul be cleanedup befor destuction!");
		}
	}

	void DebugMessenger::Initialize(Instance& instance)
	{
		if (!instance.IsEnableValidationLayers()) return;

		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		instance.PopulateDebugMessengerCreateInfo(createInfo);

		if (CreateDebugUtilsMessengerEXT(instance.GetInstance(), &createInfo, nullptr, &mDebugMessenger) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to set up debug messenger!");
		}
		std::cout << "Debug messenger created" << std::endl;
	}

	void DebugMessenger::Cleanup(Instance& instance)
	{
		if (instance.IsEnableValidationLayers()) 
		{
			DestroyDebugUtilsMessengerEXT(instance.GetInstance(), mDebugMessenger, nullptr);
		}
		mDebugMessenger = nullptr;
	}

}
