#include "MeshContainer.hpp"
#include "Mesh.hpp"

#include "HedgehogContext/Context/VulkanContext.hpp"
#include "HedgehogWrappers/Wrappeers/Device/Device.hpp"
#include "HedgehogWrappers/Wrappeers/Resources/Buffer/Buffer.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHICommandList.hpp"

#include "Scene/Scene.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <stdexcept>

namespace Context
{
namespace
{
    // Legacy: upload data via Wrappers staging buffer.
    template<typename T>
    void CreateBuffer(const VulkanContext& context, const std::vector<T>& data, std::unique_ptr<Wrappers::Buffer>& buffer)
    {
        auto& device = context.GetDevice();
        VkDeviceSize size = sizeof(T) * data.size();

        Wrappers::Buffer stagingBuffer(device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        stagingBuffer.CopyDataToBufferMemory(device, data.data(), static_cast<size_t>(size));

        VkBufferUsageFlags flags = 0;
        if (std::is_same<T, uint32_t>::value)
        {
            flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        }
        else
        {
            flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        }
        buffer = std::make_unique<Wrappers::Buffer>(device, size, flags, VMA_MEMORY_USAGE_GPU_ONLY);
        device.CopyBufferToBuffer(stagingBuffer.GetNativeBuffer(), buffer->GetNativeBuffer(), size);

        stagingBuffer.DestroyBuffer(device);
    }

    // New RHI path: upload data via RHI staging buffer.
    template<typename T>
    void CreateRHIBuffer(const VulkanContext& context, const std::vector<T>& data,
                         bool isIndex, std::unique_ptr<RHI::IRHIBuffer>& outBuffer)
    {
        auto& rhiDevice = context.GetRHIDevice();
        size_t size = sizeof(T) * data.size();

        auto stagingBuffer = rhiDevice.CreateBuffer(
            size, RHI::BufferUsage::TransferSrc, RHI::MemoryUsage::CpuToGpu);
        stagingBuffer->CopyData(data.data(), size);

        RHI::BufferUsage gpuUsage = isIndex
            ? (RHI::BufferUsage::TransferDst | RHI::BufferUsage::IndexBuffer)
            : (RHI::BufferUsage::TransferDst | RHI::BufferUsage::VertexBuffer);

        outBuffer = rhiDevice.CreateBuffer(size, gpuUsage, RHI::MemoryUsage::GpuOnly);

        rhiDevice.ExecuteImmediately([&](RHI::IRHICommandList& cmd)
        {
            cmd.CopyBufferToBuffer(*stagingBuffer, *outBuffer, 0, 0, size);
        });
    }
}

    MeshContainer::MeshContainer()
    {
    }

    MeshContainer::~MeshContainer()
    {
    }

    void MeshContainer::AddFilePath(std::string filePath)
    {
        auto it = std::find(m_FilePaths.begin(), m_FilePaths.end(), filePath);
        if (it == m_FilePaths.end())
        {
            m_FilePaths.push_back(filePath);
        }
        else
        {
            LOGWARNING("Mesh ", filePath, " already added");
        }
    }

    void MeshContainer::ClearFileList()
    {
        m_FilePaths.clear();
    }

    void MeshContainer::LoadMeshData()
    {
        m_Meshes.clear();
        for (size_t i = m_Meshes.size(); i < m_FilePaths.size(); ++i)
        {
            Mesh mesh;
            mesh.LoadData(m_FilePaths[i]);
            LOGINFO("Loaded mesh data: ", m_FilePaths[i]);
            m_Meshes.push_back(mesh);
        }
    }

    void MeshContainer::Initialize(const VulkanContext& context)
    {
        std::vector<HM::Vector3> positions;
        std::vector<HM::Vector2> texCoords;
        std::vector<HM::Vector3> normals;
        std::vector<uint32_t> indices;
        positions.clear();
        texCoords.clear();
        normals.clear();
        indices.clear();

        for (size_t i = 0; i < m_Meshes.size(); ++i)
        {
            auto& mesh = m_Meshes[i];
            mesh.SetVertexOffset(static_cast<uint32_t>(positions.size()));
            mesh.SetFirstIndex(static_cast<uint32_t>(indices.size()));

            auto meshPositions = mesh.GetPositions();
            auto meshUV = mesh.GetTexCoords();
            auto meshNormals = mesh.GetNormals();
            if (meshPositions.size() != meshUV.size() || meshUV.size() != meshNormals.size())
            {
                throw std::runtime_error("In mesh positions, texcoords and normals have different sizes!");
            }
            for (size_t j = 0; j < meshPositions.size(); ++j)
            {
                positions.push_back(meshPositions[j]);
                texCoords.push_back(meshUV[j]);
                normals.push_back(meshNormals[j]);
            }
            auto meshIndices = mesh.GetIndices();
            for (size_t j = 0; j < meshIndices.size(); ++j)
            {
                indices.push_back(meshIndices[j]);
            }
        }

        CreateBuffer(context, positions, m_AdditionalPositionsBuffer);
        CreateBuffer(context, texCoords, m_AdditionalTexCoordsBuffer);
        CreateBuffer(context, normals, m_AdditionalNormalsBuffer);
        CreateBuffer(context, indices, m_AdditionalIndexBuffer);

        CreateRHIBuffer(context, positions, /*isIndex=*/false, m_AdditionalRHIPositionsBuffer);
        CreateRHIBuffer(context, texCoords, /*isIndex=*/false, m_AdditionalRHITexCoordsBuffer);
        CreateRHIBuffer(context, normals,   /*isIndex=*/false, m_AdditionalRHINormalsBuffer);
        CreateRHIBuffer(context, indices,   /*isIndex=*/true,  m_AdditionalRHIIndexBuffer);

        m_IsSwapped = true;
    }

    void MeshContainer::Update(const VulkanContext& context, Scene::Scene& scene)
    {
        if (m_IsSwapped)
        {
            if (m_AdditionalPositionsBuffer != nullptr)
            {
                if (m_PositionsBuffer != nullptr)
                {
                    m_PositionsBuffer->DestroyBuffer(context.GetDevice());
                }
                m_PositionsBuffer = std::move(m_AdditionalPositionsBuffer);
                m_AdditionalPositionsBuffer = nullptr;
            }
            if (m_AdditionalTexCoordsBuffer != nullptr)
            {
                if (m_TexCoordsBuffer != nullptr)
                {
                    m_TexCoordsBuffer->DestroyBuffer(context.GetDevice());
                }
                m_TexCoordsBuffer = std::move(m_AdditionalTexCoordsBuffer);
                m_AdditionalTexCoordsBuffer = nullptr;
            }
            if (m_AdditionalNormalsBuffer != nullptr)
            {
                if (m_NormalsBuffer != nullptr)
                {
                    m_NormalsBuffer->DestroyBuffer(context.GetDevice());
                }
                m_NormalsBuffer = std::move(m_AdditionalNormalsBuffer);
                m_AdditionalNormalsBuffer = nullptr;
            }
            if (m_AdditionalIndexBuffer != nullptr)
            {
                if (m_IndexBuffer != nullptr)
                {
                    m_IndexBuffer->DestroyBuffer(context.GetDevice());
                }
                m_IndexBuffer = std::move(m_AdditionalIndexBuffer);
                m_AdditionalIndexBuffer = nullptr;
            }

            // Swap RHI buffers (unique_ptr reset releases the old GPU allocation).
            if (m_AdditionalRHIPositionsBuffer != nullptr)
            {
                m_RHIPositionsBuffer = std::move(m_AdditionalRHIPositionsBuffer);
                m_AdditionalRHIPositionsBuffer = nullptr;
            }
            if (m_AdditionalRHITexCoordsBuffer != nullptr)
            {
                m_RHITexCoordsBuffer = std::move(m_AdditionalRHITexCoordsBuffer);
                m_AdditionalRHITexCoordsBuffer = nullptr;
            }
            if (m_AdditionalRHINormalsBuffer != nullptr)
            {
                m_RHINormalsBuffer = std::move(m_AdditionalRHINormalsBuffer);
                m_AdditionalRHINormalsBuffer = nullptr;
            }
            if (m_AdditionalRHIIndexBuffer != nullptr)
            {
                m_RHIIndexBuffer = std::move(m_AdditionalRHIIndexBuffer);
                m_AdditionalRHIIndexBuffer = nullptr;
            }

            m_IsSwapped = false;
        }
        auto& meshes = scene.GetMeshes();
        if (meshes.size() <= m_Meshes.size())
            return;
        for (size_t i = m_Meshes.size(); i < meshes.size(); ++i)
        {
            AddFilePath(meshes[i]);
        }
        LoadMeshData();
        Initialize(context);
    }

    void MeshContainer::Cleanup(const VulkanContext& context)
    {
        auto& device = context.GetDevice();
        if (m_PositionsBuffer != nullptr)
            m_PositionsBuffer->DestroyBuffer(device);
        if (m_TexCoordsBuffer != nullptr)
            m_TexCoordsBuffer->DestroyBuffer(device);
        if (m_NormalsBuffer != nullptr)
            m_NormalsBuffer->DestroyBuffer(device);
        if (m_IndexBuffer != nullptr)
            m_IndexBuffer->DestroyBuffer(device);
        if (m_AdditionalPositionsBuffer != nullptr)
            m_AdditionalPositionsBuffer->DestroyBuffer(device);
        if (m_AdditionalTexCoordsBuffer != nullptr)
            m_AdditionalTexCoordsBuffer->DestroyBuffer(device);
        if (m_AdditionalNormalsBuffer != nullptr)
            m_AdditionalNormalsBuffer->DestroyBuffer(device);
        if (m_AdditionalIndexBuffer != nullptr)
            m_AdditionalIndexBuffer->DestroyBuffer(device);

        // RHI buffers are destroyed when unique_ptrs go out of scope.
        // Reset explicitly here so the device is still alive when destructors run.
        context.GetRHIDevice().WaitIdle();
        m_RHIPositionsBuffer.reset();
        m_RHITexCoordsBuffer.reset();
        m_RHINormalsBuffer.reset();
        m_RHIIndexBuffer.reset();
        m_AdditionalRHIPositionsBuffer.reset();
        m_AdditionalRHITexCoordsBuffer.reset();
        m_AdditionalRHINormalsBuffer.reset();
        m_AdditionalRHIIndexBuffer.reset();
    }

    const VkBuffer& MeshContainer::GetPositionsBuffer() const
    {
        if (!m_IsSwapped)
            return m_PositionsBuffer->GetNativeBuffer();
        return m_AdditionalPositionsBuffer->GetNativeBuffer();
    }

    const VkBuffer& MeshContainer::GetTexCoordsBuffer() const
    {
        if (!m_IsSwapped)
            return m_TexCoordsBuffer->GetNativeBuffer();
        return m_AdditionalTexCoordsBuffer->GetNativeBuffer();
    }

    const VkBuffer& MeshContainer::GetNormalsBuffer() const
    {
        if (!m_IsSwapped)
            return m_NormalsBuffer->GetNativeBuffer();
        return m_AdditionalNormalsBuffer->GetNativeBuffer();
    }

    const VkBuffer& MeshContainer::GetIndexBuffer() const
    {
        if (!m_IsSwapped)
            return m_IndexBuffer->GetNativeBuffer();
        return m_AdditionalIndexBuffer->GetNativeBuffer();
    }

    const RHI::IRHIBuffer& MeshContainer::GetRHIPositionsBuffer() const
    {
        if (!m_IsSwapped)
            return *m_RHIPositionsBuffer;
        return *m_AdditionalRHIPositionsBuffer;
    }

    const RHI::IRHIBuffer& MeshContainer::GetRHITexCoordsBuffer() const
    {
        if (!m_IsSwapped)
            return *m_RHITexCoordsBuffer;
        return *m_AdditionalRHITexCoordsBuffer;
    }

    const RHI::IRHIBuffer& MeshContainer::GetRHINormalsBuffer() const
    {
        if (!m_IsSwapped)
            return *m_RHINormalsBuffer;
        return *m_AdditionalRHINormalsBuffer;
    }

    const RHI::IRHIBuffer& MeshContainer::GetRHIIndexBuffer() const
    {
        if (!m_IsSwapped)
            return *m_RHIIndexBuffer;
        return *m_AdditionalRHIIndexBuffer;
    }

    const Mesh& MeshContainer::GetMesh(size_t index) const
    {
        return m_Meshes[index];
    }


}



