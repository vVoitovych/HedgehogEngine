#pragma once

#include <vulkan/vulkan.h>

#include <vector>
#include <string>
#include <memory>

namespace Wrappers
{
    class Buffer;
}

namespace RHI
{
    class IRHIBuffer;
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

        // Legacy Wrappers API (kept until all render passes are migrated).
        const VkBuffer& GetPositionsBuffer() const;
        const VkBuffer& GetTexCoordsBuffer() const;
        const VkBuffer& GetNormalsBuffer() const;
        const VkBuffer& GetIndexBuffer() const;

        // New RHI API.
        const RHI::IRHIBuffer& GetRHIPositionsBuffer() const;
        const RHI::IRHIBuffer& GetRHITexCoordsBuffer() const;
        const RHI::IRHIBuffer& GetRHINormalsBuffer() const;
        const RHI::IRHIBuffer& GetRHIIndexBuffer() const;

        const Mesh& GetMesh(size_t index) const;

    private:
        void AddFilePath(std::string filePath);
        void ClearFileList();

        void LoadMeshData();

        void Initialize(const VulkanContext& context);

    private:
        std::vector<std::string> m_FilePaths;
        std::vector<Mesh> m_Meshes;

        bool m_IsSwapped = false;

        // Legacy Wrappers objects (removed once all render passes are migrated).
        std::unique_ptr<Wrappers::Buffer> m_PositionsBuffer;
        std::unique_ptr<Wrappers::Buffer> m_TexCoordsBuffer;
        std::unique_ptr<Wrappers::Buffer> m_NormalsBuffer;
        std::unique_ptr<Wrappers::Buffer> m_IndexBuffer;

        std::unique_ptr<Wrappers::Buffer> m_AdditionalPositionsBuffer;
        std::unique_ptr<Wrappers::Buffer> m_AdditionalTexCoordsBuffer;
        std::unique_ptr<Wrappers::Buffer> m_AdditionalNormalsBuffer;
        std::unique_ptr<Wrappers::Buffer> m_AdditionalIndexBuffer;

        // RHI objects — authoritative going forward.
        std::unique_ptr<RHI::IRHIBuffer> m_RHIPositionsBuffer;
        std::unique_ptr<RHI::IRHIBuffer> m_RHITexCoordsBuffer;
        std::unique_ptr<RHI::IRHIBuffer> m_RHINormalsBuffer;
        std::unique_ptr<RHI::IRHIBuffer> m_RHIIndexBuffer;

        std::unique_ptr<RHI::IRHIBuffer> m_AdditionalRHIPositionsBuffer;
        std::unique_ptr<RHI::IRHIBuffer> m_AdditionalRHITexCoordsBuffer;
        std::unique_ptr<RHI::IRHIBuffer> m_AdditionalRHINormalsBuffer;
        std::unique_ptr<RHI::IRHIBuffer> m_AdditionalRHIIndexBuffer;

    };

}





