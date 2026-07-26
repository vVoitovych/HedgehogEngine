#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace Renderer
{
    class IRenderPass;
    struct PassInitContext;
    struct NodeDesc;

    // Type-string -> constructor factory, so a .rgq node's `type:` field can name any pass without
    // RenderGraphInstantiator needing a switch/dynamic_cast over concrete pass types (RTTI is
    // unused anywhere in this codebase). Populated once, in Renderer's constructor, with the seven
    // built-in pass types.
    class RenderPassRegistry
    {
    public:
        using PassFactory = std::function<std::unique_ptr<IRenderPass>(const PassInitContext&, const NodeDesc&)>;

        void Register(std::string typeId, PassFactory factory);

        [[nodiscard]] bool IsRegistered(const std::string& typeId) const;

        // Returns nullptr + LOGERROR if typeId isn't registered. RenderGraphLoader's V4 already
        // rejects an unregistered type at parse time, so this is a defensive backstop, not the
        // primary error path.
        [[nodiscard]] std::unique_ptr<IRenderPass> Create(const std::string& typeId,
                                                           const PassInitContext& init,
                                                           const NodeDesc& node) const;

    private:
        std::unordered_map<std::string, PassFactory> m_Factories;
    };
}
