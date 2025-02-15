#pragma once

#include "Mesh.hpp"

#include <vulkan/vulkan.h>

#include <vector>
#include <string>
#include <memory>

namespace Wrappers
{
	class Buffer;
}

namespace Scene
{
	class Scene;
}

namespace Context
{
	class VulkanContext;
	class Mesh;

	class MeshContainer
	{
	public:
		MeshContainer();
		~MeshContainer();

		MeshContainer(const MeshContainer&) = delete;
		MeshContainer(MeshContainer&&) = delete;
		MeshContainer& operator=(const MeshContainer&) = delete;
		MeshContainer& operator=(MeshContainer&&) = delete;

		void Update(const VulkanContext& context, Scene::Scene& scene);

		void Cleanup(const VulkanContext& context);

		const VkBuffer& GetVertexBuffer() const;
		const VkBuffer& GetIndexBuffer() const;

		const Mesh& GetMesh(size_t index) const;

	private:
		void AddFilePath(std::string filePath);
		void ClearFileList();

		void LoadMeshData();

		void Initialize(const VulkanContext& context);
		void CreateVertexBuffer(const VulkanContext& context, const std::vector<VertexDescription> verticies);
		void CreateIndexBuffer(const VulkanContext& context, const std::vector<uint32_t> indicies);

	private:
		std::vector<std::string> mFilePathes;
		std::vector<Mesh> mMeshes;

	private:
		std::unique_ptr<Wrappers::Buffer> mVertexBuffer;

		std::unique_ptr<Wrappers::Buffer> mIndexBuffer;

	};

}





