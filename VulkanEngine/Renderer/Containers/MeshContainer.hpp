#pragma once

#include "Mesh.hpp"

#include <vulkan/vulkan.h>

#include <vector>
#include <string>

namespace Renderer
{
	class Device;

	class MeshContainer
	{
	public:
		void AddFilePath(std::string filePath);
		void ClearFileList();

		void LoadMeshData();
		void LoadSingleMesh(std::string filePath);

		void Initialize(const Device& device);
		void Cleanup(const Device& device);

		VkBuffer GetVertexBuffer();
		VkBuffer GetIndexBuffer();

	private:
		void CreateVertexBuffer(const Device& device, const std::vector<VertexDescription> verticies);
		void CreateIndexBuffer(const Device& device, const std::vector<uint32_t> indicies);

	private:
		std::vector<std::string> mFilePathes;
		std::vector<Mesh> mMeshes;

	private:
		VkBuffer mVertexBuffer;
		VkDeviceMemory mVertexBufferMemory;

		VkBuffer mIndexBuffer;
		VkDeviceMemory mIndexBufferMemory;

	};

}





