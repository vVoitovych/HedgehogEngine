#include <vulkan/vulkan.h>
#include <unordered_map>
#include <unordered_set>
#include <string>

namespace Renderer
{
	enum class ResourceType
	{
		ForwardTarget,
		ForwardDepth
	};

	class Device;

	class ResourceTracker
	{
	public:
		ResourceTracker() = default;
		~ResourceTracker() = default;

		void Initialise(const Device& device);
		void Cleanup(const Device& device);

		VkImageView GetResource(const ResourceType& type);

	private:
		std::unordered_map<std::string, VkImageView> mLoadedTextures;
		std::unordered_map<ResourceType, VkImageView> mRenderTargets;
		std::unordered_set<ResourceType> mScreenSizeDependentResources;
	};

}

