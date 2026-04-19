#include "MeshContainer.hpp"
#include "Mesh.hpp"

#include "HedgehogContext/Context/VulkanContext.hpp"

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

        CreateRHIBuffer(context, positions, false, m_AdditionalRHIPositionsBuffer);
        CreateRHIBuffer(context, texCoords, false, m_AdditionalRHITexCoordsBuffer);
        CreateRHIBuffer(context, normals,   false, m_AdditionalRHINormalsBuffer);
        CreateRHIBuffer(context, indices,   true,  m_AdditionalRHIIndexBuffer);

        m_IsSwapped = true;
    }

    void MeshContainer::Update(const VulkanContext& context, Scene::Scene& scene)
    {
        if (m_IsSwapped)
        {
            if (m_AdditionalRHIPositionsBuffer)
                m_RHIPositionsBuffer = std::move(m_AdditionalRHIPositionsBuffer);
            if (m_AdditionalRHITexCoordsBuffer)
                m_RHITexCoordsBuffer = std::move(m_AdditionalRHITexCoordsBuffer);
            if (m_AdditionalRHINormalsBuffer)
                m_RHINormalsBuffer = std::move(m_AdditionalRHINormalsBuffer);
            if (m_AdditionalRHIIndexBuffer)
                m_RHIIndexBuffer = std::move(m_AdditionalRHIIndexBuffer);

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
