#include "MeshContainer.hpp"
#include "Mesh.hpp"

#include "HedgehogContext/Context/VulkanContext.hpp"
#include "HedgehogWrappers/Wrappeers/Device/Device.hpp"
#include "HedgehogWrappers/Wrappeers/Resources/Buffer/Buffer.hpp"

#include "Scene/Scene.hpp"

#include "Logger/Logger.hpp"

#include <algorithm>
#include <stdexcept>

namespace Context
{
    template<typename T>
    void CreateBuffer(const VulkanContext& context, const std::vector<T> data, std::unique_ptr<Wrappers::Buffer>& buffer)
    {
        auto& device = context.GetDevice();
        VkDeviceSize size = sizeof(T) * data.size();

        Wrappers::Buffer staginBuffer(device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        staginBuffer.CopyDataToBufferMemory(device, data.data(), (size_t)size);

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
        device.CopyBufferToBuffer(staginBuffer.GetNativeBuffer(), buffer->GetNativeBuffer(), size);

        staginBuffer.DestroyBuffer(device);
    }


    MeshContainer::MeshContainer()
    {
    }

    MeshContainer::~MeshContainer()
    {
    }

    void MeshContainer::AddFilePath(std::string filePath)
    {
        auto it = std::find(m_FilePathes.begin(), m_FilePathes.end(), filePath);
        if (it == m_FilePathes.end())
        {
            m_FilePathes.push_back(filePath);
        }
        else
        {
            LOGWARNING("Mesh ", filePath, " already added");
        }
    }

    void MeshContainer::ClearFileList()
    {
        m_FilePathes.clear();
    }

    void MeshContainer::LoadMeshData()
    {
        m_Meshes.clear();
        for (size_t i = m_Meshes.size(); i < m_FilePathes.size(); ++i)
        {
            Mesh mesh;
            mesh.LoadData(m_FilePathes[i]);
            LOGINFO("Loaded mesh data: ", m_FilePathes[i]);
            m_Meshes.push_back(mesh);

        }
    }

    void MeshContainer::Initialize(const VulkanContext& context)
    {
        std::vector<HM::Vector3> positions;
        std::vector<HM::Vector2> texCoords;
        std::vector<HM::Vector3> normals;
        std::vector<uint32_t> indicies;
        positions.clear();
        texCoords.clear();
        normals.clear();
        indicies.clear();

        for (size_t i = 0; i < m_Meshes.size(); ++i)
        {
            auto& mesh = m_Meshes[i];
            mesh.SetVertexOffset(static_cast<uint32_t>(positions.size()));
            mesh.SetFirstIndex(static_cast<uint32_t>(indicies.size()));

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
            auto meshIndicies = mesh.GetIndicies();
            for (size_t j = 0; j < meshIndicies.size(); ++j)
            {
                indicies.push_back(meshIndicies[j]);
            }
        }

        CreateBuffer(context, positions, m_AdditionalPositionsBuffer);
        CreateBuffer(context, texCoords, m_AdditionalTexCoordsBuffer);
        CreateBuffer(context, normals, m_AdditionalNormalsBuffer);
        CreateBuffer(context, indicies, m_AdditionalIndexBuffer);

        m_IsSwaped = true;
    }

    void MeshContainer::Update(const VulkanContext& context, Scene::Scene& scene)
    {
        if (m_IsSwaped)
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
            m_IsSwaped = false;
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
    }

    const VkBuffer& MeshContainer::GetPositionsBuffer() const
    {
        if (!m_IsSwaped)
            return m_PositionsBuffer->GetNativeBuffer();
        return m_AdditionalPositionsBuffer->GetNativeBuffer();
    }

    const VkBuffer& MeshContainer::GetTexCoordsBuffer() const
    {
        if (!m_IsSwaped)
            return m_TexCoordsBuffer->GetNativeBuffer();
        return m_AdditionalTexCoordsBuffer->GetNativeBuffer();
    }

    const VkBuffer& MeshContainer::GetNormalsBuffer() const
    {
        if (!m_IsSwaped)
            return m_NormalsBuffer->GetNativeBuffer();
        return m_AdditionalNormalsBuffer->GetNativeBuffer();
    }

    const VkBuffer& MeshContainer::GetIndexBuffer() const
    {
        if (!m_IsSwaped)
            return m_IndexBuffer->GetNativeBuffer();
        return m_AdditionalIndexBuffer->GetNativeBuffer();
    }

    const Mesh& MeshContainer::GetMesh(size_t index) const
    {
        return m_Meshes[index];
    }


}




