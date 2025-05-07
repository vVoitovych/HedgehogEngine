#pragma once

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

		const VkBuffer& GetPositionsBuffer() const;
		const VkBuffer& GetTexCoordsBuffer() const;
		const VkBuffer& GetNormalsBuffer() const;
		const VkBuffer& GetIndexBuffer() const;

		const Mesh& GetMesh(size_t index) const;

	private:
		void AddFilePath(std::string filePath);
		void ClearFileList();

		void LoadMeshData();

		void Initialize(const VulkanContext& context);

	private:
		std::vector<std::string> m_FilePathes;
		std::vector<Mesh> m_Meshes;

		bool m_IsSwaped = false;

		std::unique_ptr<Wrappers::Buffer> m_PositionsBuffer;
		std::unique_ptr<Wrappers::Buffer> m_TexCoordsBuffer;
		std::unique_ptr<Wrappers::Buffer> m_NormalsBuffer;
		std::unique_ptr<Wrappers::Buffer> m_IndexBuffer;

		std::unique_ptr<Wrappers::Buffer> m_AdditionalPositionsBuffer;
		std::unique_ptr<Wrappers::Buffer> m_AdditionalTexCoordsBuffer;
		std::unique_ptr<Wrappers::Buffer> m_AdditionalNormalsBuffer;
		std::unique_ptr<Wrappers::Buffer> m_AdditionalIndexBuffer;

	};

}





