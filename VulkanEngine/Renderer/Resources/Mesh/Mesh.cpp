#include "Mesh.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "VulkanEngine/Libraries/TinyObjectLoader/tiny_obj_loader.h"

#include "VulkanEngine/Renderer/Device/Device.h"
#include "VulkanEngine/Logger/Logger.h"
#include "VulkanEngine/ContentLoader/CommonFunctions.h"

#include <unordered_map>

namespace Renderer
{
	Mesh::Mesh()
	{
		mVertices = Vertex::GetQuad();
		mIndices = Vertex::GetQuadIndecies();
	}

	Mesh::~Mesh()
	{
	}

	void Mesh::LoadModel(const std::string fileName)
	{
		mVertices.clear();
		mIndices.clear();

		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string warn, err;

		std::string modelPath = ContentLoader::GetAssetsDirectory() + fileName;

		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, modelPath.c_str())) 
		{
			throw std::runtime_error(warn + err);
		}

		std::unordered_map<Vertex, uint32_t> uniqueVertices{};

		for (const auto& shape : shapes) {
			for (const auto& index : shape.mesh.indices) {
				Vertex vertex{};

				vertex.pos = 
				{
					attrib.vertices[3 * index.vertex_index + 0],
					attrib.vertices[3 * index.vertex_index + 1],
					attrib.vertices[3 * index.vertex_index + 2]
				};

				vertex.texCoord = 
				{
					attrib.texcoords[2 * index.texcoord_index + 0],
					1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
				};

				vertex.color = { 0.0f, 0.0f, 0.0f };

				if (uniqueVertices.count(vertex) == 0) 
				{
					uniqueVertices[vertex] = static_cast<uint32_t>(mVertices.size());
					mVertices.push_back(vertex);
				}

				mIndices.push_back(uniqueVertices[vertex]);
			}
		}
		LOGINFO("Model loaded with ", mVertices.size(), " verticies and ", mIndices.size(), " indicies!");
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
		return static_cast<uint32_t>(mIndices.size());
	}

	void Mesh::CreateVertexBuffer(const Device& device)
	{
		VkDeviceSize size = sizeof(mVertices[0]) * mVertices.size();

		VkBuffer staginBuffer;
		VkDeviceMemory staginBufferMemory;
		device.CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,	staginBuffer, staginBufferMemory);
		device.CopyDataToBufferMemory(staginBufferMemory, mVertices.data(), (size_t)size);

		device.CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mVertexBuffer, mVertexBufferMemory);
		device.CopyBuffer(staginBuffer, mVertexBuffer, size);

		device.DestroyBuffer(staginBuffer, nullptr);
		device.FreeMemory(staginBufferMemory, nullptr);
		LOGINFO("Vertex buffer created");
	}

	void Mesh::CreateIndexBuffer(const Device& device)
	{
		VkDeviceSize size = sizeof(mIndices[0]) * mIndices.size();

		VkBuffer staginBuffer;
		VkDeviceMemory staginBufferMemory;
		device.CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,	staginBuffer, staginBufferMemory);
		device.CopyDataToBufferMemory(staginBufferMemory, mIndices.data(), (size_t)size);

		device.CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			mIndexBuffer, mIndexBufferMemory);
		device.CopyBuffer(staginBuffer, mIndexBuffer, size);

		device.DestroyBuffer(staginBuffer, nullptr);
		device.FreeMemory(staginBufferMemory, nullptr);
		LOGINFO("Index buffer created");
	}

	
}


