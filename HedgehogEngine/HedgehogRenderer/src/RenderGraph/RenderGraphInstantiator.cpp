#include "RenderGraphInstantiator.hpp"

#include "RenderGraphDesc.hpp"
#include "RenderPassRegistry.hpp"
#include "IRenderPass.hpp"

#include "Logger/api/Logger.hpp"

namespace Renderer
{
    bool RenderGraphInstantiator::Instantiate(const GraphAssetDesc& desc,
                                              const RenderPassRegistry& registry,
                                              const PassInitContext& initCtx,
                                              std::vector<std::unique_ptr<IRenderPass>>& outPasses)
    {
        for (const NodeDesc& node : desc.Nodes)
        {
            if (!node.Enabled)
                continue;

            auto pass = registry.Create(node.Type, initCtx, node);
            if (!pass)
            {
                LOGERROR("RenderGraphInstantiator: failed to construct node '", node.Instance,
                         "' of type '", node.Type, "'.");
                return false;
            }

            outPasses.push_back(std::move(pass));
        }

        return true;
    }
}
