#pragma once

#include "Mesh.hpp"

#include <vulkan/vulkan.h>

#include <vector>
#include <string>
#include <memory>

namespace Renderer
{
	class Device;
	class CommandPool;
	class Buffer;

	class MeshContainer
	{
	public:
		MeshContainer();
		~MeshContainer();

		MeshContainer(const MeshContainer&) = delete;
		MeshContainer(MeshContainer&&) = delete;
		MeshContainer& operator=(const MeshContainer&) = delete;
		MeshContainer& operator=(MeshContainer&&) = delete;

		void AddFilePath(std::string filePath);
		void ClearFileList();

		void LoadMeshData();
		void LoadSingleMesh(std::string filePath);

		void Initialize(const std::unique_ptr<Device>& device, const std::unique_ptr<CommandPool>& commandPool);
		void Cleanup(const std::unique_ptr<Device>& device);

		const VkBuffer& GetVertexBuffer() const;
		const VkBuffer& GetIndexBuffer() const;

		const Mesh& GetMesh(size_t index) const;

	private:
		void CreateVertexBuffer(const std::unique_ptr<Device>& device, const std::unique_ptr<CommandPool>& commandPool, const std::vector<VertexDescription> verticies);
		void CreateIndexBuffer(const std::unique_ptr<Device>& device, const std::unique_ptr<CommandPool>& commandPool, const std::vector<uint32_t> indicies);

	private:
		std::vector<std::string> mFilePathes;
		std::vector<Mesh> mMeshes;

	private:
		std::unique_ptr<Buffer> mVertexBuffer;

		std::unique_ptr<Buffer> mIndexBuffer;

	};

}





