#pragma once

#include <memory>
#include <string>
#include <unordered_map>

namespace RHI
{
    class IRHIDevice;
}

namespace Renderer
{
    struct PassInitContext;

    // String-keyed cache of a pass's SHARED, IMMUTABLE resources (render pass, pipeline,
    // descriptor-set layouts, the one-time ResourceRegistry::SetMaterialLayout call — see
    // PassInitContext). Every per-view instance of a pass type calls GetOrCreate<T>() with the
    // same key; only the first call constructs T, every later call — from any other instance —
    // returns the same shared_ptr. This is what lets several per-view pass instances exist
    // without re-registering a material descriptor-set layout: ResourceRegistry::SetMaterialLayout
    // asserts it is called at most once. See workflow/current-plan.md, "Pass instance sharing".
    class PassResourceCache
    {
    public:
        PassResourceCache()  = default;
        ~PassResourceCache() = default;

        PassResourceCache(const PassResourceCache&)            = delete;
        PassResourceCache& operator=(const PassResourceCache&) = delete;
        PassResourceCache(PassResourceCache&&)                 = delete;
        PassResourceCache& operator=(PassResourceCache&&)      = delete;

        // Returns the cached instance for `key`, constructing it as std::make_shared<T>(init) on
        // first use. Every later call with the same key — from any pass instance — returns the
        // same object without re-running T's constructor.
        template<typename T>
        std::shared_ptr<T> GetOrCreate(const std::string& key, const PassInitContext& init)
        {
            const auto it = m_Entries.find(key);
            if (it != m_Entries.end())
                return std::static_pointer_cast<T>(it->second);

            auto created = std::make_shared<T>(init);
            m_Entries[key] = created;
            return created;
        }

        // Releases every cached entry. Call once every pass instance holding a shared_ptr into
        // this cache has already released its own (i.e. after every pass's own Cleanup()), so
        // this call drops the last reference and each resource's RHI destructor actually runs.
        void Cleanup(RHI::IRHIDevice& device);

    private:
        std::unordered_map<std::string, std::shared_ptr<void>> m_Entries;
    };
}
