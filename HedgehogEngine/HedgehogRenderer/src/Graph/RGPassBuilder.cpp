#include "HedgehogRenderer/Graph/RGPassBuilder.hpp"

#include "HedgehogRenderer/Graph/GraphBuilder.hpp"

#include <cassert>

namespace Renderer
{
    void RGPassBuilder::SampleTexture(RGTexture texture)
    {
        RecordRead(texture.Id, texture.Version, RGResourceUsage::SampledTexture);
    }

    RGTexture RGPassBuilder::ColorTarget(RGTexture texture)
    {
        const RGVersion newVersion = RecordWrite(texture.Id, texture.Version, RGResourceUsage::ColorTarget);
        return RGTexture{ texture.Id, newVersion };
    }

    RGTexture RGPassBuilder::DepthTarget(RGTexture texture)
    {
        const RGVersion newVersion = RecordWrite(texture.Id, texture.Version, RGResourceUsage::DepthTarget);
        return RGTexture{ texture.Id, newVersion };
    }

    void RGPassBuilder::DepthReadOnly(RGTexture texture)
    {
        RecordRead(texture.Id, texture.Version, RGResourceUsage::DepthReadOnly);
    }

    RGTexture RGPassBuilder::StorageWrite(RGTexture texture)
    {
        const RGVersion newVersion = RecordWrite(texture.Id, texture.Version, RGResourceUsage::StorageReadWrite);
        return RGTexture{ texture.Id, newVersion };
    }

    RGBuffer RGPassBuilder::StorageWrite(RGBuffer buffer)
    {
        const RGVersion newVersion = RecordWrite(buffer.Id, buffer.Version, RGResourceUsage::StorageReadWrite);
        return RGBuffer{ buffer.Id, newVersion };
    }

    void RGPassBuilder::ReadBuffer(RGBuffer buffer)
    {
        RecordRead(buffer.Id, buffer.Version, RGResourceUsage::ReadBuffer);
    }

    RGBuffer RGPassBuilder::WriteBuffer(RGBuffer buffer)
    {
        const RGVersion newVersion = RecordWrite(buffer.Id, buffer.Version, RGResourceUsage::WriteBuffer);
        return RGBuffer{ buffer.Id, newVersion };
    }

    void RGPassBuilder::SetSideEffect()
    {
        m_Builder.GetPassRecordMutable(m_PassIndex).HasSideEffect = true;
    }

    void RGPassBuilder::RecordRead(RGResourceId id, RGVersion version, RGResourceUsage usage)
    {
        RGResourceRef ref;
        ref.Id      = id;
        ref.Version = version;
        ref.Usage   = usage;
        m_Builder.GetPassRecordMutable(m_PassIndex).Reads.push_back(ref);
    }

    RGVersion RGPassBuilder::RecordWrite(RGResourceId id, RGVersion inputVersion, RGResourceUsage usage)
    {
        RGResourceRecord& resource = m_Builder.GetResourceRecordMutable(id);
        // Deliberately NOT asserting on IsImported && IsReadOnly here: GraphCompiler::Compile
        // is where that is reported as a proper CompileError (with pass and resource names),
        // and it needs to be possible to declare the mistake in the first place to test that.
        assert(inputVersion == resource.LatestVersion
               && "Writing a stale handle — this isn't the latest version of the resource. "
                  "Thread the handle a prior write/read verb returned, not an old one.");

        const RGVersion newVersion = ++resource.LatestVersion;

        RGResourceRef ref;
        ref.Id      = id;
        ref.Version = newVersion;
        ref.Usage   = usage;
        m_Builder.GetPassRecordMutable(m_PassIndex).Writes.push_back(ref);

        return newVersion;
    }
}
