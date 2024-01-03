#pragma once

#include "Mesh.hpp"

#include <vulkan/vulkan.h>

#include <vector>
#include <string>
#include <memory>

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

		void Initialize(const std::unique_ptr<Device>& device);
		void Cleanup(const std::unique_ptr<Device>& device);

		VkBuffer GetVertexBuffer();
		VkBuffer GetIndexBuffer();

		Mesh& GetMesh(size_t index);

	private:
		void CreateVertexBuffer(const std::unique_ptr<Device>& device, const std::vector<VertexDescription> verticies);
		void CreateIndexBuffer(const std::unique_ptr<Device>& device, const std::vector<uint32_t> indicies);

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





