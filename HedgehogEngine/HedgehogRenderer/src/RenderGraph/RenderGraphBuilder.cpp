#include "RenderGraphBuilder.hpp"

#include "RenderGraph.hpp"

namespace Renderer
{
    RenderGraphBuilder::RenderGraphBuilder(RenderGraph& graph, size_t passIndex)
        : m_Graph(graph)
        , m_PassIndex(passIndex)
    {
    }

    ResourceHandle RenderGraphBuilder::CreateTexture(const std::string& name, const GraphTextureDesc& desc)
    {
        return m_Graph.DeclareTexture(name, desc);
    }

    ResourceHandle RenderGraphBuilder::Write(const std::string& name, RHI::ImageLayout finalLayoutAfterExecute)
    {
        return m_Graph.DeclareWrite(m_PassIndex, name, finalLayoutAfterExecute);
    }

    ResourceHandle RenderGraphBuilder::Read(const std::string& name)
    {
        return m_Graph.DeclareRead(m_PassIndex, name);
    }

    ResourceHandle RenderGraphBuilder::ReadSampled(const std::string& name)
    {
        return m_Graph.DeclareReadSampled(m_PassIndex, name);
    }

    ResourceHandle RenderGraphBuilder::ImportWrite(const std::string& scopedName, RHI::ImageLayout finalLayoutAfterExecute)
    {
        return m_Graph.DeclareImportWrite(m_PassIndex, scopedName, finalLayoutAfterExecute);
    }

    ResourceHandle RenderGraphBuilder::ImportRead(const std::string& scopedName)
    {
        return m_Graph.DeclareImportRead(m_PassIndex, scopedName);
    }

    ResourceHandle RenderGraphBuilder::ImportReadSampled(const std::string& scopedName)
    {
        return m_Graph.DeclareImportReadSampled(m_PassIndex, scopedName);
    }
}
