#include "Mesh.h"

#include "VulkanEngine/Renderer/Device/Device.h"
#include "VulkanEngine/Renderer/Commands/CommandPool.h"
#include "VulkanEngine/Renderer/Common/CommonFunctions.h"
#include "VulkanEngine/Logger/Logger.h"

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
		mDevice = device.GetNativeDevice();
		mPhysicalDevice = device.GetNativePhysicalDevice();
		mGraphicsQueue = device.GetNativeGraphicsQueue();
		mCommandPool = commandPool.GetNativeCommandPool();

		CreateVertexBuffer();
		CreateIndexBuffer();
	}

	void Mesh::Cleanup()
	{
		vkDestroyBuffer(mDevice, mVertexBuffer, nullptr);
		vkFreeMemory(mDevice, mVertexBufferMemory, nullptr);
		mVertexBuffer = nullptr;
		mVertexBufferMemory = nullptr;
		LOGINFO("Vertex buffer cleaned");

		vkDestroyBuffer(mDevice, mIndexBuffer, nullptr);
		vkFreeMemory(mDevice, mIndexBufferMemory, nullptr);
		mIndexBuffer = nullptr;
		mIndexBufferMemory = nullptr;
		LOGINFO("Index buffer cleaned");
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

	void Mesh::CreateVertexBuffer()
	{
		VkDeviceSize size = sizeof(mVerticies[0]) * mVerticies.size();

		VkBuffer staginBuffer;
		VkDeviceMemory staginBufferMemory;
		CreateBuffer(mDevice, mPhysicalDevice, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			staginBuffer, staginBufferMemory);

		void* data;
		vkMapMemory(mDevice, staginBufferMemory, 0, size, 0, &data);
		memcpy(data, mVerticies.data(), (size_t)size);
		vkUnmapMemory(mDevice, staginBufferMemory);

		CreateBuffer(mDevice, mPhysicalDevice, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			mVertexBuffer, mVertexBufferMemory);
		CopyBuffer(mDevice, mCommandPool, mGraphicsQueue, staginBuffer, mVertexBuffer, size);

		vkDestroyBuffer(mDevice, staginBuffer, nullptr);
		vkFreeMemory(mDevice, staginBufferMemory, nullptr);
		LOGINFO("Vertex buffer created");
	}

	void Mesh::CreateIndexBuffer()
	{
		VkDeviceSize size = sizeof(mIndicies[0]) * mIndicies.size();

		VkBuffer staginBuffer;
		VkDeviceMemory staginBufferMemory;
		CreateBuffer(mDevice, mPhysicalDevice, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			staginBuffer, staginBufferMemory);

		void* data;
		vkMapMemory(mDevice, staginBufferMemory, 0, size, 0, &data);
		memcpy(data, mIndicies.data(), (size_t)size);
		vkUnmapMemory(mDevice, staginBufferMemory);

		CreateBuffer(mDevice, mPhysicalDevice, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			mIndexBuffer, mIndexBufferMemory);
		CopyBuffer(mDevice, mCommandPool, mGraphicsQueue, staginBuffer, mIndexBuffer, size);

		vkDestroyBuffer(mDevice, staginBuffer, nullptr);
		vkFreeMemory(mDevice, staginBufferMemory, nullptr);
		LOGINFO("Index buffer created");
	}

	
}


