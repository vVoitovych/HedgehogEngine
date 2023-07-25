#include "Mesh.h"
#include "..\VulkanAPIWrappers\Device.h"
#include "..\VulkanAPIWrappers\CommandPool.h"

namespace Renderer
{
	Mesh::Mesh()
	{
		mVerticies = Vertex::GetQuad();
		mIndicies = Vertex::GetQuadIndecies();
	}

	Mesh::~Mesh()
	{
	}

	void Mesh::Initialize(Device& device, CommandPool& commandPool)
	{
		CreateVertexBuffer(device, commandPool);
		CreateIndexBuffer(device, commandPool);
	}

	void Mesh::Cleanup(Device& device)
	{
		vkDestroyBuffer(device.GetDevice(), mVertexBuffer, nullptr);
		vkFreeMemory(device.GetDevice(), mVertexBufferMemory, nullptr);

		vkDestroyBuffer(device.GetDevice(), mIndexBuffer, nullptr);
		vkFreeMemory(device.GetDevice(), mIndexBufferMemory, nullptr);
	}

	VkBuffer Mesh::GetVertexBuffer()
	{
		return mVertexBuffer;
	}

	VkBuffer Mesh::GetIndexBuffer()
	{
		return mIndexBuffer;
	}

	uint32_t Mesh::GetIndiciesCount()
	{
		return static_cast<uint32_t>(mIndicies.size());
	}

	void Mesh::CreateVertexBuffer(Device& device, CommandPool& commandPool)
	{
		VkDeviceSize size = sizeof(mVerticies[0]) * mVerticies.size();

		VkBuffer staginBuffer;
		VkDeviceMemory staginBufferMemory;
		device.CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			staginBuffer, staginBufferMemory);

		void* data;
		vkMapMemory(device.GetDevice(), staginBufferMemory, 0, size, 0, &data);
		memcpy(data, mVerticies.data(), (size_t)size);
		vkUnmapMemory(device.GetDevice(), staginBufferMemory);

		device.CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			mVertexBuffer, mVertexBufferMemory);
		device.CopyBuffer(staginBuffer, mVertexBuffer, size, commandPool);

		vkDestroyBuffer(device.GetDevice(), staginBuffer, nullptr);
		vkFreeMemory(device.GetDevice(), staginBufferMemory, nullptr);
	}

	void Mesh::CreateIndexBuffer(Device& device, CommandPool& commandPool)
	{
		VkDeviceSize size = sizeof(mIndicies[0]) * mIndicies.size();

		VkBuffer staginBuffer;
		VkDeviceMemory staginBufferMemory;
		device.CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			staginBuffer, staginBufferMemory);

		void* data;
		vkMapMemory(device.GetDevice(), staginBufferMemory, 0, size, 0, &data);
		memcpy(data, mIndicies.data(), (size_t)size);
		vkUnmapMemory(device.GetDevice(), staginBufferMemory);

		device.CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			mIndexBuffer, mIndexBufferMemory);
		device.CopyBuffer(staginBuffer, mIndexBuffer, size, commandPool);

		vkDestroyBuffer(device.GetDevice(), staginBuffer, nullptr);
		vkFreeMemory(device.GetDevice(), staginBufferMemory, nullptr);
	}

}


