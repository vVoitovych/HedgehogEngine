#include "HedgehogRenderer/Graph/GraphCompiler.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <set>
#include <unordered_map>

namespace Renderer
{
    namespace
    {
        uint64_t PackKey(RGResourceId id, RGVersion version)
        {
            return (static_cast<uint64_t>(id) << 32) | static_cast<uint64_t>(version);
        }

        using ProducerMap = std::unordered_map<uint64_t, size_t>;               // (id,version) -> pass index that wrote it
        using ReadersMap  = std::unordered_map<uint64_t, std::vector<size_t>>;  // (id,version) -> passes that read it

        std::string ResourceDisplayName(const GraphDescription& description, RGResourceId id)
        {
            const RGResourceRecord* resource = description.FindResource(id);
            return resource ? resource->Name : ("resource#" + std::to_string(id));
        }

        ProducerMap BuildProducerMap(const GraphDescription& description)
        {
            ProducerMap producerOf;
            for (size_t p = 0; p < description.Passes.size(); ++p)
                for (const RGResourceRef& write : description.Passes[p].Writes)
                    producerOf[PackKey(write.Id, write.Version)] = p;
            return producerOf;
        }

        ReadersMap BuildReadersMap(const GraphDescription& description)
        {
            ReadersMap readersOf;
            for (size_t p = 0; p < description.Passes.size(); ++p)
                for (const RGResourceRef& read : description.Passes[p].Reads)
                    readersOf[PackKey(read.Id, read.Version)].push_back(p);
            return readersOf;
        }

        // Step 6 (the part that doesn't need a DAG): read-with-no-producer, write to an
        // imported read-only resource, unbound output slots. Cycle detection happens later,
        // inside the topological sort, since it needs the DAG built.
        std::vector<CompileError> ValidateStatic(const GraphDescription& description,
                                                   const ProducerMap&      producerOf)
        {
            std::vector<CompileError> errors;

            for (const RGPassRecord& pass : description.Passes)
            {
                for (const RGResourceRef& read : pass.Reads)
                {
                    if (read.Version == 0)
                        continue; // version 0 is the resource's initial state — always valid to read
                    if (producerOf.find(PackKey(read.Id, read.Version)) != producerOf.end())
                        continue;

                    const std::string resourceName = ResourceDisplayName(description, read.Id);
                    CompileError error;
                    error.PassName     = pass.Name;
                    error.ResourceName = resourceName;
                    error.Message = "Pass '" + pass.Name + "' reads version " + std::to_string(read.Version)
                                  + " of resource '" + resourceName + "', but no pass produced that version.";
                    errors.push_back(std::move(error));
                }

                for (const RGResourceRef& write : pass.Writes)
                {
                    const RGResourceRecord* resource = description.FindResource(write.Id);
                    if (!resource || !(resource->IsImported && resource->IsReadOnly))
                        continue;

                    CompileError error;
                    error.PassName     = pass.Name;
                    error.ResourceName = resource->Name;
                    error.Message = "Pass '" + pass.Name + "' writes imported read-only resource '"
                                  + resource->Name + "'.";
                    errors.push_back(std::move(error));
                }
            }

            for (size_t slotIndex = 0; slotIndex < description.OutputSlots.size(); ++slotIndex)
            {
                const RGOutputSlot& slot = description.OutputSlots[slotIndex];
                if (slot.IsBound)
                    continue;

                const std::string slotName = slot.Name.empty()
                    ? ("slot#" + std::to_string(slotIndex)) : slot.Name;
                CompileError error;
                error.ResourceName = slotName;
                error.Message = "Output slot " + std::to_string(slotIndex) + " ('" + slotName
                              + "') was never bound (GraphBuilder::BindOutput).";
                errors.push_back(std::move(error));
            }

            return errors;
        }

        // Step 1: recover dependency edges purely from handle versions. RAW edges (this pass
        // needs a version someone else produced) matter for both culling and ordering; WAR
        // edges (this pass overwrites a version someone else is still reading) matter only for
        // ordering — culling must never treat a WAR edge as "the reader is needed".
        void BuildEdges(const GraphDescription& description,
                         const ProducerMap&       producerOf,
                         const ReadersMap&        readersOf,
                         std::vector<std::vector<size_t>>& outRawDeps,
                         std::vector<std::vector<size_t>>& outWarDeps)
        {
            const size_t passCount = description.Passes.size();
            outRawDeps.assign(passCount, {});
            outWarDeps.assign(passCount, {});

            for (size_t p = 0; p < passCount; ++p)
            {
                const RGPassRecord& pass = description.Passes[p];

                for (const RGResourceRef& read : pass.Reads)
                {
                    if (read.Version == 0)
                        continue;
                    auto it = producerOf.find(PackKey(read.Id, read.Version));
                    if (it != producerOf.end() && it->second != p)
                        outRawDeps[p].push_back(it->second);
                }

                for (const RGResourceRef& write : pass.Writes)
                {
                    // A write verb always bumps the version by exactly one (RGPassBuilder::RecordWrite),
                    // so the version it supersedes is always Version - 1.
                    const RGVersion supersededVersion = write.Version - 1;

                    if (supersededVersion > 0)
                    {
                        auto it = producerOf.find(PackKey(write.Id, supersededVersion));
                        if (it != producerOf.end() && it->second != p)
                            outRawDeps[p].push_back(it->second);
                    }

                    auto rit = readersOf.find(PackKey(write.Id, supersededVersion));
                    if (rit != readersOf.end())
                    {
                        for (size_t reader : rit->second)
                        {
                            if (reader != p)
                                outWarDeps[p].push_back(reader);
                        }
                    }
                }
            }
        }

        // Step 2: cull. A pass is live if it has a side effect, or produces a version an
        // output slot is bound to, or (transitively, via RAW edges only) is needed by a live
        // pass's reads.
        std::vector<bool> ComputeLiveness(const GraphDescription&                  description,
                                            const ProducerMap&                        producerOf,
                                            const std::vector<std::vector<size_t>>&   rawDeps)
        {
            const size_t passCount = description.Passes.size();
            std::vector<bool>   isLive(passCount, false);
            std::vector<size_t> worklist;

            auto markLive = [&](size_t p)
            {
                if (!isLive[p])
                {
                    isLive[p] = true;
                    worklist.push_back(p);
                }
            };

            for (size_t p = 0; p < passCount; ++p)
                if (description.Passes[p].HasSideEffect)
                    markLive(p);

            for (const RGOutputSlot& slot : description.OutputSlots)
            {
                if (!slot.IsBound)
                    continue;
                auto it = producerOf.find(PackKey(slot.BoundResource, slot.BoundVersion));
                if (it != producerOf.end())
                    markLive(it->second);
            }

            while (!worklist.empty())
            {
                const size_t p = worklist.back();
                worklist.pop_back();
                for (size_t dep : rawDeps[p])
                    markLive(dep);
            }

            return isLive;
        }

        // Step 3: topological sort of the live subgraph (Kahn's algorithm), tie-breaking toward
        // whichever ready pass shares the most resources with the most recently scheduled pass
        // — keeps a run of passes on the same render target adjacent instead of interleaving
        // unrelated work in an arbitrary but technically-valid order. Detects cycles as a
        // byproduct: if any live pass never reaches in-degree 0, it's part of one.
        std::optional<CompileError> TopologicalSort(const GraphDescription&                description,
                                                       const std::vector<bool>&               isLive,
                                                       const std::vector<std::vector<size_t>>& rawDeps,
                                                       const std::vector<std::vector<size_t>>& warDeps,
                                                       std::vector<size_t>&                    outOrder)
        {
            const size_t passCount = description.Passes.size();

            std::vector<size_t>              inDegree(passCount, 0);
            std::vector<std::vector<size_t>> dependents(passCount);

            auto addEdge = [&](size_t before, size_t after)
            {
                if (!isLive[before] || !isLive[after])
                    return;
                dependents[before].push_back(after);
                inDegree[after]++;
            };

            for (size_t p = 0; p < passCount; ++p)
            {
                if (!isLive[p])
                    continue;
                std::set<size_t> deps(rawDeps[p].begin(), rawDeps[p].end());
                deps.insert(warDeps[p].begin(), warDeps[p].end());
                for (size_t dep : deps)
                    addEdge(dep, p);
            }

            std::vector<size_t> ready;
            size_t liveCount = 0;
            for (size_t p = 0; p < passCount; ++p)
            {
                if (!isLive[p])
                    continue;
                ++liveCount;
                if (inDegree[p] == 0)
                    ready.push_back(p);
            }

            std::vector<RGResourceId> lastTouched;
            outOrder.clear();
            outOrder.reserve(liveCount);

            while (!ready.empty())
            {
                size_t bestSlot  = 0;
                int    bestScore = -1;
                for (size_t i = 0; i < ready.size(); ++i)
                {
                    const size_t p = ready[i];
                    int score = 0;
                    for (const RGResourceRef& r : description.Passes[p].Reads)
                        score += static_cast<int>(std::count(lastTouched.begin(), lastTouched.end(), r.Id));
                    for (const RGResourceRef& w : description.Passes[p].Writes)
                        score += static_cast<int>(std::count(lastTouched.begin(), lastTouched.end(), w.Id));

                    // Tie-break on original declaration order for determinism.
                    if (score > bestScore || (score == bestScore && p < ready[bestSlot]))
                    {
                        bestScore = score;
                        bestSlot  = i;
                    }
                }

                const size_t chosen = ready[bestSlot];
                ready.erase(ready.begin() + static_cast<std::ptrdiff_t>(bestSlot));
                outOrder.push_back(chosen);

                lastTouched.clear();
                for (const RGResourceRef& r : description.Passes[chosen].Reads)
                    lastTouched.push_back(r.Id);
                for (const RGResourceRef& w : description.Passes[chosen].Writes)
                    lastTouched.push_back(w.Id);

                for (size_t dependent : dependents[chosen])
                {
                    if (--inDegree[dependent] == 0)
                        ready.push_back(dependent);
                }
            }

            if (outOrder.size() != liveCount)
            {
                std::string names;
                for (size_t p = 0; p < passCount; ++p)
                {
                    if (isLive[p] && inDegree[p] > 0)
                    {
                        if (!names.empty())
                            names += ", ";
                        names += "'" + description.Passes[p].Name + "'";
                    }
                }

                CompileError error;
                error.PassName = description.Passes.empty() ? "" : names;
                error.Message  = "Cycle detected among passes that depend on each other: " + names + ".";
                return error;
            }

            return std::nullopt;
        }

        // Step 5: derive barriers in execution order, tracking each resource's current state.
        // All transitions a single pass needs — across every resource it touches — land in that
        // pass's own TextureBarriers/BufferBarriers, i.e. one batched Barrier() call, not one
        // call per resource.
        void DeriveBarriers(const GraphDescription& description, CompiledGraph& graph)
        {
            std::unordered_map<RGResourceId, RHI::ResourceState> currentState;

            for (CompiledPass& compiled : graph.Passes)
            {
                const RGPassRecord& pass = description.Passes[compiled.OriginalPassIndex];

                std::vector<RGResourceRef> touches;
                touches.reserve(pass.Reads.size() + pass.Writes.size());
                touches.insert(touches.end(), pass.Reads.begin(), pass.Reads.end());
                touches.insert(touches.end(), pass.Writes.begin(), pass.Writes.end());

                for (const RGResourceRef& touch : touches)
                {
                    const RHI::ResourceState newState = ToResourceState(touch.Usage);
                    auto it = currentState.find(touch.Id);
                    const RHI::ResourceState oldState = (it != currentState.end())
                        ? it->second : RHI::ResourceState::Undefined;

                    if (oldState == newState)
                        continue;

                    const RGResourceRecord* resource = description.FindResource(touch.Id);
                    const bool isBuffer = resource && resource->IsBuffer;

                    if (isBuffer)
                    {
                        RGBufferBarrier barrier;
                        barrier.Id     = touch.Id;
                        barrier.Before = oldState;
                        barrier.After  = newState;
                        compiled.BufferBarriers.push_back(barrier);
                    }
                    else
                    {
                        RGTextureBarrier barrier;
                        barrier.Id     = touch.Id;
                        barrier.Before = oldState;
                        barrier.After  = newState;
                        compiled.TextureBarriers.push_back(barrier);
                    }

                    currentState[touch.Id] = newState;
                }
            }
        }

        // Step 4: first/last use per physical resource, over the final (culled, sorted) order.
        void ComputeLifetimes(const GraphDescription& description, CompiledGraph& graph)
        {
            std::unordered_map<RGResourceId, ResourceLifetime> lifetimes;

            for (size_t sortedIndex = 0; sortedIndex < graph.Passes.size(); ++sortedIndex)
            {
                const RGPassRecord& pass = description.Passes[graph.Passes[sortedIndex].OriginalPassIndex];

                auto touch = [&](RGResourceId id)
                {
                    auto it = lifetimes.find(id);
                    if (it == lifetimes.end())
                    {
                        ResourceLifetime lifetime;
                        lifetime.Id           = id;
                        lifetime.FirstUsePass = sortedIndex;
                        lifetime.LastUsePass  = sortedIndex;
                        lifetimes.emplace(id, lifetime);
                    }
                    else
                    {
                        it->second.LastUsePass = sortedIndex;
                    }
                };

                for (const RGResourceRef& r : pass.Reads)
                    touch(r.Id);
                for (const RGResourceRef& w : pass.Writes)
                    touch(w.Id);
            }

            graph.ResourceLifetimes.reserve(lifetimes.size());
            for (auto& [id, lifetime] : lifetimes)
                graph.ResourceLifetimes.push_back(lifetime);
        }
    }

    CompileResult GraphCompiler::Compile(const GraphDescription& description) const
    {
        CompileResult result;

        const ProducerMap producerOf = BuildProducerMap(description);
        const ReadersMap  readersOf  = BuildReadersMap(description);

        std::vector<CompileError> staticErrors = ValidateStatic(description, producerOf);
        if (!staticErrors.empty())
        {
            result.Errors = std::move(staticErrors);
            return result;
        }

        std::vector<std::vector<size_t>> rawDeps;
        std::vector<std::vector<size_t>> warDeps;
        BuildEdges(description, producerOf, readersOf, rawDeps, warDeps);

        const std::vector<bool> isLive = ComputeLiveness(description, producerOf, rawDeps);

        std::vector<size_t> sortedOrder;
        std::optional<CompileError> cycleError =
            TopologicalSort(description, isLive, rawDeps, warDeps, sortedOrder);
        if (cycleError)
        {
            result.Errors.push_back(std::move(*cycleError));
            return result;
        }

        CompiledGraph graph;
        graph.Passes.reserve(sortedOrder.size());
        for (size_t originalIndex : sortedOrder)
        {
            CompiledPass compiled;
            compiled.OriginalPassIndex = originalIndex;
            compiled.Name              = description.Passes[originalIndex].Name;
            graph.Passes.push_back(std::move(compiled));
        }

        DeriveBarriers(description, graph);
        ComputeLifetimes(description, graph);

        result.Success = true;
        result.Graph   = std::move(graph);
        return result;
    }
}
