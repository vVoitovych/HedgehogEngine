#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace Wrappers
{
	class Instance
	{
	public:
		Instance();
		~Instance();

		Instance(const Instance&) = delete;
		Instance(Instance&&) = delete;
		Instance& operator=(const Instance&) = delete;
		Instance& operator=(Instance&&) = delete;

		void Cleanup();

		VkInstance GetNativeInstance();
		const VkInstance GetNativeInstance() const;
		const std::vector<const char*>& GetLayers() const;
#ifdef DEBUG
		VkDebugUtilsMessengerCreateInfoEXT GetDebugMessangerInfo() const;
#endif
	private:
		void InitializeLayers();
		bool CheckLayersSupport() const;
		std::vector<const char*> GetRequiredExtensions() const;
		void CheckInstanceExtensions() const;

	private:
		VkInstance m_Instance;
		std::vector<const char*> m_Layers;
#ifdef DEBUG
		VkDebugUtilsMessengerCreateInfoEXT m_DebugMessangerInfo;
#endif
	};

}


