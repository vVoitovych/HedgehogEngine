#include "ResourceContainer.hpp"

#include "VulkanEngine/Renderer/Wrappeers/Device/Device.hpp"

namespace Renderer
{
	void ResourceTracker::Initialise(const Device& device)
	{
	}

	void ResourceTracker::Cleanup(const Device& device)
	{
	}

	VkImageView ResourceTracker::GetResource(const ResourceType& type)
	{
		return VkImageView();
	}

}


