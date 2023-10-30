#include "Mesh.h"

#include "VulkanEngine/Renderer/Device/Device.h"
#include "VulkanEngine/Logger/Logger.h"

namespace Renderer
{
	Mesh::Mesh()
	{
		mVerticies = Vertex::GetCubeVirticies();
		mIndicies = Vertex::GetCubeIndecies();
	}

	Mesh::~Mesh()
	{
	}

	void Mesh::Initialize(const Device& device)
	{
		CreateVertexBuffer(device);
		CreateIndexBuffer(device);
	}

	void Mesh::Cleanup(const Device& device)
	{
		vkDestroyBuffer(device.GetNativeDevice(), mVertexBuffer, nullptr);
		vkFreeMemory(device.GetNativeDevice(), mVertexBufferMemory, nullptr);
		mVertexBuffer = nullptr;
		mVertexBufferMemory = nullptr;
		LOGINFO("Vertex buffer cleaned");

		vkDestroyBuffer(device.GetNativeDevice(), mIndexBuffer, nullptr);
		vkFreeMemory(device.GetNativeDevice(), mIndexBufferMemory, nullptr);
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

	void Mesh::CreateVertexBuffer(const Device& device)
	{
		VkDeviceSize size = sizeof(mVerticies[0]) * mVerticies.size();

		VkBuffer staginBuffer;
		VkDeviceMemory staginBufferMemory;
		device.CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,	staginBuffer, staginBufferMemory);
		device.CopyDataToBufferMemory(staginBufferMemory, mVerticies.data(), (size_t)size);

		device.CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mVertexBuffer, mVertexBufferMemory);
		device.CopyBuffer(staginBuffer, mVertexBuffer, size);

		device.DestroyBuffer(staginBuffer, nullptr);
		device.FreeMemory(staginBufferMemory, nullptr);
		LOGINFO("Vertex buffer created");
	}

	void Mesh::CreateIndexBuffer(const Device& device)
	{
		VkDeviceSize size = sizeof(mIndicies[0]) * mIndicies.size();

		VkBuffer staginBuffer;
		VkDeviceMemory staginBufferMemory;
		device.CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,	staginBuffer, staginBufferMemory);
		device.CopyDataToBufferMemory(staginBufferMemory, mIndicies.data(), (size_t)size);

		device.CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			mIndexBuffer, mIndexBufferMemory);
		device.CopyBuffer(staginBuffer, mIndexBuffer, size);

		device.DestroyBuffer(staginBuffer, nullptr);
		device.FreeMemory(staginBufferMemory, nullptr);
		LOGINFO("Index buffer created");
	}

	
}


