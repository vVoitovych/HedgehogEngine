#include "HedgehogRenderer/Graph/GraphBuilder.hpp"

#include "HedgehogRenderer/Graph/RGPassBuilder.hpp"

#include <cassert>

namespace Renderer
{
    RGTexture GraphBuilder::CreateTexture(const RGTextureDesc& desc)
    {
        RGResourceRecord record;
        record.Id       = AllocateResourceId();
        record.Name     = desc.Name;
        record.IsBuffer = false;
        record.TextureDesc = desc;
        m_Description.Resources.push_back(record);
        return RGTexture{ record.Id, 0 };
    }

    RGBuffer GraphBuilder::CreateBuffer(const RGBufferDesc& desc)
    {
        RGResourceRecord record;
        record.Id       = AllocateResourceId();
        record.Name     = desc.Name;
        record.IsBuffer = true;
        record.BufferDesc = desc;
        m_Description.Resources.push_back(record);
        return RGBuffer{ record.Id, 0 };
    }

    RGTexture GraphBuilder::ImportTexture(const std::string& name, RHI::Format format, bool isReadOnly)
    {
        RGResourceRecord record;
        record.Id         = AllocateResourceId();
        record.Name       = name;
        record.IsImported = true;
        record.IsReadOnly = isReadOnly;
        record.IsBuffer   = false;
        record.TextureDesc.Name   = name;
        record.TextureDesc.Format = format;
        m_Description.Resources.push_back(record);
        return RGTexture{ record.Id, 0 };
    }

    RGBuffer GraphBuilder::ImportBuffer(const std::string& name, size_t size, bool isReadOnly)
    {
        RGResourceRecord record;
        record.Id         = AllocateResourceId();
        record.Name       = name;
        record.IsImported = true;
        record.IsReadOnly = isReadOnly;
        record.IsBuffer   = true;
        record.BufferDesc.Name = name;
        record.BufferDesc.Size = size;
        m_Description.Resources.push_back(record);
        return RGBuffer{ record.Id, 0 };
    }

    uint32_t GraphBuilder::AddOutputSlot(const std::string& name, RHI::Format format, const RGSizePolicy& size)
    {
        RGOutputSlot slot;
        slot.Name   = name;
        slot.Format = format;
        slot.Size   = size;
        m_Description.OutputSlots.push_back(slot);
        return static_cast<uint32_t>(m_Description.OutputSlots.size() - 1);
    }

    void GraphBuilder::AddPass(const std::string& name, const std::function<void(RGPassBuilder&)>& setup)
    {
        RGPassRecord record;
        record.Name = name;
        m_Description.Passes.push_back(record);

        const size_t passIndex = m_Description.Passes.size() - 1;
        RGPassBuilder passBuilder(*this, passIndex);
        if (setup)
            setup(passBuilder);
    }

    RGResourceRecord& GraphBuilder::GetResourceRecordMutable(RGResourceId id)
    {
        for (auto& resource : m_Description.Resources)
        {
            if (resource.Id == id)
                return resource;
        }
        assert(false && "RGResourceId not found — handle from a different GraphBuilder?");
        static RGResourceRecord dummy;
        return dummy;
    }

    RGPassRecord& GraphBuilder::GetPassRecordMutable(size_t passIndex)
    {
        assert(passIndex < m_Description.Passes.size());
        return m_Description.Passes[passIndex];
    }
}
