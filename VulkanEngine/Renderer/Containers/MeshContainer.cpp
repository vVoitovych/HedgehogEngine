#include "MeshContainer.hpp"

#include "Renderer/Wrappeers/Device/Device.hpp"
#include "Renderer/Wrappeers/Commands/CommandPool.hpp"
#include "Renderer/Wrappeers/Resources/Buffer/Buffer.hpp"

#include "Logger/Logger.hpp"

#include <algorithm>

namespace Renderer
{
    MeshContainer::MeshContainer()
    {
    }

    MeshContainer::~MeshContainer()
    {
    }

    void MeshContainer::AddFilePath(std::string filePath)
    {
        auto it = std::find(mFilePathes.begin(), mFilePathes.end(), filePath);
        if (it == mFilePathes.end())
        {
            mFilePathes.push_back(filePath);
        }
        else
        {
            LOGWARNING("Mesh ", filePath, " already added");
        }
    }

    void MeshContainer::ClearFileList()
    {
        mFilePathes.clear();
    }

    void MeshContainer::LoadMeshData()
    {
        mMeshes.clear();
        for (size_t i = 0; i < mFilePathes.size(); ++i)
        {
            Mesh mesh;
            mesh.LoadData(mFilePathes[i]);
            LOGINFO("Loaded mesh data: ", mFilePathes[i]);
            mMeshes.push_back(mesh);

        }
    }

    void MeshContainer::LoadSingleMesh(std::string filePath)
    {
        auto it = std::find(mFilePathes.begin(), mFilePathes.end(), filePath);
        if (it == mFilePathes.end())
            return;
        mFilePathes.push_back(filePath);

        Mesh mesh;        
        mesh.LoadData(filePath);
        LOGINFO("Loaded mesh data: ", filePath);
        mMeshes.push_back(mesh);

    }

    void MeshContainer::Initialize(const std::unique_ptr<Device>& device, const std::unique_ptr<CommandPool>& commandPool)
    {
        std::vector<VertexDescription> verticies;
        std::vector<uint32_t> indicies;
        verticies.clear();
        indicies.clear();

        for (size_t i = 0; i < mMeshes.size(); ++i)
        {
            auto& mesh = mMeshes[i];
            mesh.SetVertexOffset(static_cast<uint32_t>(verticies.size()));
            mesh.SetFirstIndex(static_cast<uint32_t>(indicies.size()));

            auto meshVerticies = mesh.GetVerticies();
            for (size_t j = 0; j < meshVerticies.size(); ++j)
            {
                verticies.push_back(meshVerticies[j]);
            }
            auto meshIndicies = mesh.GetIndicies();
            for (size_t j = 0; j < meshIndicies.size(); ++j)
            {
                indicies.push_back(meshIndicies[j]);
            }
        }

        CreateVertexBuffer(device, commandPool, verticies);
        CreateIndexBuffer(device, commandPool, indicies);
    }

    void MeshContainer::Cleanup(const std::unique_ptr<Device>& device)
    {
        mVertexBuffer->DestroyBuffer(device);
        LOGINFO("Vertex buffer cleaned");

        mIndexBuffer->DestroyBuffer(device);
        LOGINFO("Index buffer cleaned");
    }

    const VkBuffer& MeshContainer::GetVertexBuffer() const
    {
        return mVertexBuffer->GetNativeBuffer();
    }

    const VkBuffer& MeshContainer::GetIndexBuffer() const
    {
        return mIndexBuffer->GetNativeBuffer();
    }

    const Mesh& MeshContainer::GetMesh(size_t index) const
    {
        return mMeshes[index];
    }

    void MeshContainer::CreateVertexBuffer(const std::unique_ptr<Device>& device, const std::unique_ptr<CommandPool>& commandPool, const std::vector<VertexDescription> verticies)
    {
        VkDeviceSize size = sizeof(verticies[0]) * verticies.size();

        Buffer staginBuffer(device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        staginBuffer.CopyDataToBufferMemory(device, verticies.data(), (size_t)size);

        mVertexBuffer = std::make_unique<Buffer>(device, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
        staginBuffer.CopyBuffer(mVertexBuffer->GetNativeBuffer(), size, commandPool);

        staginBuffer.DestroyBuffer(device);
        LOGINFO("Vertex buffer created");
    }

    void MeshContainer::CreateIndexBuffer(const std::unique_ptr<Device>& device, const std::unique_ptr<CommandPool>& commandPool, const std::vector<uint32_t> indicies)
    {
        VkDeviceSize size = sizeof(indicies[0]) * indicies.size();

        Buffer staginBuffer(device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        staginBuffer.CopyDataToBufferMemory(device, indicies.data(), (size_t)size);

        mIndexBuffer = std::make_unique<Buffer>(device, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
        staginBuffer.CopyBuffer(mIndexBuffer->GetNativeBuffer(), size, commandPool);

        staginBuffer.DestroyBuffer(device);
        LOGINFO("Index buffer created");
    }

}




