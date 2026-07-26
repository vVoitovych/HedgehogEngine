#pragma once

#include "RHI/api/RHITypes.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace RHI
{
    class IRHITexture;
}

namespace Renderer
{
    // Frame-level, cross-graph-instance registry of every published transient texture, keyed by
    // its scoped name ("shared/shadowDepth", "scene/viewColor", "game/viewColor", ...). Each
    // RenderGraph instance publishes the textures it owns here after (re)creating them, and
    // resolves resources it imports from another graph's scope here too. Layout tracking lives
    // here — not inside any one RenderGraph — so a resource produced by one graph and read by
    // several others gets exactly one barrier and stays correct across graph boundaries. See
    // workflow/current-plan.md, "RenderResourceLedger — the cross-stage resource mechanism".
    class RenderResourceLedger
    {
    public:
        RenderResourceLedger()  = default;
        ~RenderResourceLedger() = default;

        RenderResourceLedger(const RenderResourceLedger&)            = delete;
        RenderResourceLedger& operator=(const RenderResourceLedger&) = delete;
        RenderResourceLedger(RenderResourceLedger&&)                 = delete;
        RenderResourceLedger& operator=(RenderResourceLedger&&)      = delete;

        // Publishes (or republishes, after a resize) the texture backing scopedName, bumping its
        // generation so importers can detect the change via GetGeneration() / RenderGraph::RefreshImports.
        void Publish(const std::string& scopedName, RHI::IRHITexture* texture);

        [[nodiscard]] RHI::IRHITexture* Resolve(const std::string& scopedName) const;

        [[nodiscard]] uint32_t GetGeneration(const std::string& scopedName) const;

        [[nodiscard]] RHI::ImageLayout GetLayout(const std::string& scopedName) const;
        void SetLayout(const std::string& scopedName, RHI::ImageLayout layout);

        // Removes every entry scoped under scopePrefix (i.e. keyed "scopePrefix/..."). Called by
        // RenderGraph::Cleanup() so a destroyed graph's textures can never be resolved as a stale
        // pointer by an importer, and so a later graph reusing the same scope starts clean.
        void Erase(const std::string& scopePrefix);

    private:
        struct LedgerEntry
        {
            RHI::IRHITexture* Texture       = nullptr;  // non-owning; the declaring graph's pool owns it
            RHI::ImageLayout  CurrentLayout = RHI::ImageLayout::Undefined;
            uint32_t          Generation    = 0;         // bumped whenever Texture is (re)published
        };

        LedgerEntry& getOrCreate(const std::string& scopedName);

    private:
        std::unordered_map<std::string, LedgerEntry> m_Entries;
    };
}
