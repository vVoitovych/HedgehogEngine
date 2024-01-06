#include "MeshContainer.hpp"

#include "VulkanEngine/Renderer/Wrappeers/Device/Device.hpp"
#include "VulkanEngine/Logger/Logger.hpp"

#include <algorithm>

namespace Renderer
{
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

    void MeshContainer::Initialize(const std::unique_ptr<Device>& device)
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

        CreateVertexBuffer(device, verticies);
        CreateIndexBuffer(device, indicies);
    }

    void MeshContainer::Cleanup(const std::unique_ptr<Device>& device)
    {
        vkDestroyBuffer(device->GetNativeDevice(), mVertexBuffer, nullptr);
        vkFreeMemory(device->GetNativeDevice(), mVertexBufferMemory, nullptr);
        mVertexBuffer = nullptr;
        mVertexBufferMemory = nullptr;
        LOGINFO("Vertex buffer cleaned");

        vkDestroyBuffer(device->GetNativeDevice(), mIndexBuffer, nullptr);
        vkFreeMemory(device->GetNativeDevice(), mIndexBufferMemory, nullptr);
        mIndexBuffer = nullptr;
        mIndexBufferMemory = nullptr;
        LOGINFO("Index buffer cleaned");
    }

    VkBuffer MeshContainer::GetVertexBuffer()
    {
        return mVertexBuffer;
    }

    VkBuffer MeshContainer::GetIndexBuffer()
    {
        return mIndexBuffer;
    }

    Mesh& MeshContainer::GetMesh(size_t index)
    {
        return mMeshes[index];
    }

    void MeshContainer::CreateVertexBuffer(const std::unique_ptr<Device>& device, const std::vector<VertexDescription> verticies)
    {
        VkDeviceSize size = sizeof(verticies[0]) * verticies.size();

        VkBuffer staginBuffer;
        VkDeviceMemory staginBufferMemory;
        device->CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staginBuffer, staginBufferMemory);
        device->CopyDataToBufferMemory(staginBufferMemory, verticies.data(), (size_t)size);

        device->CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mVertexBuffer, mVertexBufferMemory);
        device->CopyBuffer(staginBuffer, mVertexBuffer, size);

        device->DestroyBuffer(staginBuffer, nullptr);
        device->FreeMemory(staginBufferMemory, nullptr);
        LOGINFO("Vertex buffer created");
    }

    void MeshContainer::CreateIndexBuffer(const std::unique_ptr<Device>& device, const std::vector<uint32_t> indicies)
    {
        VkDeviceSize size = sizeof(indicies[0]) * indicies.size();

        VkBuffer staginBuffer;
        VkDeviceMemory staginBufferMemory;
        device->CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staginBuffer, staginBufferMemory);
        device->CopyDataToBufferMemory(staginBufferMemory, indicies.data(), (size_t)size);

        device->CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            mIndexBuffer, mIndexBufferMemory);
        device->CopyBuffer(staginBuffer, mIndexBuffer, size);

        device->DestroyBuffer(staginBuffer, nullptr);
        device->FreeMemory(staginBufferMemory, nullptr);
        LOGINFO("Index buffer created");
    }

}




