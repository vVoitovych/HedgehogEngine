#pragma once

#include "../Common/pch.h"

namespace Renderer
{
	class Instance
	{
	public:
		Instance();
		~Instance();

		Instance(const Instance&) = delete;
		Instance& operator=(const Instance&) = delete;

		void Initialize();
		void Cleanup();

		VkInstance GetInstance();
		bool IsEnableValidationLayers() const;
		void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) const;
	private:
		bool CheckValidationLayerSupport() const;
		std::vector<const char*> GetRequiredExtensions() const;
		void HasGflwRequiredInstanceExtensions() const;

	private:
#ifdef DEBUG
		const bool enableValidationLayers = true;
#else
		const bool enableValidationLayers = false;
#endif
		VkInstance mInstance;

	};

}

