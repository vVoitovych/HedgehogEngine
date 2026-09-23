#pragma once

#include "RGTypes.hpp"

#include <cstddef>

namespace Renderer
{
    class GraphBuilder;

    // Passed into a pass's setup callback by GraphBuilder::AddPass. Every write verb
    // (ColorTarget, DepthTarget, StorageWrite, WriteBuffer) bumps the resource's version and
    // returns the new handle; every read verb (SampleTexture, DepthReadOnly, ReadBuffer) just
    // records which version the pass depends on. SetSideEffect flags the pass as one a later
    // compiler must not cull even if nothing reads its outputs.
    class RGPassBuilder
    {
    public:
        RGPassBuilder(GraphBuilder& builder, size_t passIndex)
            : m_Builder(builder), m_PassIndex(passIndex)
        {
        }

        RGPassBuilder(const RGPassBuilder&)            = delete;
        RGPassBuilder& operator=(const RGPassBuilder&) = delete;
        RGPassBuilder(RGPassBuilder&&)                 = delete;
        RGPassBuilder& operator=(RGPassBuilder&&)      = delete;

        void      SampleTexture(RGTexture texture);
        RGTexture ColorTarget(RGTexture texture);
        RGTexture DepthTarget(RGTexture texture);
        void      DepthReadOnly(RGTexture texture);
        RGTexture StorageWrite(RGTexture texture);
        RGBuffer  StorageWrite(RGBuffer buffer);
        void      ReadBuffer(RGBuffer buffer);
        RGBuffer  WriteBuffer(RGBuffer buffer);

        void SetSideEffect();

    private:
        void      RecordRead(RGResourceId id, RGVersion version, RGResourceUsage usage);
        RGVersion RecordWrite(RGResourceId id, RGVersion inputVersion, RGResourceUsage usage);

        GraphBuilder& m_Builder;
        size_t        m_PassIndex;
    };
}
