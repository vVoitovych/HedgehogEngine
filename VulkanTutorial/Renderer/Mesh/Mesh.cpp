#include "Mesh.h"
#include "..\VulkanAPIWrappers\Device.h"

namespace Renderer
{
	Mesh::Mesh()
	{
		mVerticies = Vertex::GetSimpleTriangle();
	}

	Mesh::~Mesh()
	{
	}

	void Mesh::CreateVertexBuffer(Device& device)
	{
		VkBufferCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		info.size = sizeof(mVerticies[0]) * mVerticies.size();
		info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateBuffer(device.GetDevice(), &info, nullptr, &mVertexBuffer) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create vertex buffer");
		}

		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(device.GetDevice(), mVertexBuffer, &memRequirements);

		VkMemoryAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.allocationSize = memRequirements.size;
		allocateInfo.memoryTypeIndex = device.FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		if (vkAllocateMemory(device.GetDevice(), &allocateInfo, nullptr, &mVertexBufferMemory) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to allocate memory!");
		}

		vkBindBufferMemory(device.GetDevice(), mVertexBuffer, mVertexBufferMemory, 0);

		void* data;
		vkMapMemory(device.GetDevice(), mVertexBufferMemory, 0, info.size, 0, &data);
		memcpy(data, mVerticies.data(), (size_t)info.size);
		vkUnmapMemory(device.GetDevice(), mVertexBufferMemory);
	}

	void Mesh::Cleanup(Device& device)
	{
		vkDestroyBuffer(device.GetDevice(), mVertexBuffer, nullptr);
		vkFreeMemory(device.GetDevice(), mVertexBufferMemory, nullptr);
	}

	VkBuffer Mesh::GetVertexBuffer()
	{
		return mVertexBuffer;
	}

}


