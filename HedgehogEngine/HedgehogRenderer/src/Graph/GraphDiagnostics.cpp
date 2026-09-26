#include "HedgehogRenderer/Graph/GraphDiagnostics.hpp"

#include "HedgehogRenderer/Graph/GraphCompiler.hpp"
#include "HedgehogRenderer/Graph/GraphInstantiator.hpp"
#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"

namespace Renderer
{
    namespace
    {
        // Pass data and closures of one graph's declaration; the engine's graphs use a few KB.
        constexpr size_t SCRATCH_ARENA_BYTES = 64 * 1024;
    }

    std::vector<GraphDiagnostic> DiagnoseGraphAsset(const GraphAsset& asset, const PassBuilderRegistry& registry,
                                                    RHI::IRHIDevice& device)
    {
        std::vector<GraphDiagnostic> diagnostics;

        RenderGraphRuntime graph(device, SCRATCH_ARENA_BYTES);
        GraphImports imports;
        for (const GraphAssetImport& import : asset.Imports)
            imports.emplace(import.Name, graph.ImportTexture(import.Name, import.Format, true));

        // Instantiate validates first and declares nothing when validation fails.
        const GraphInstantiationResult instantiated = GraphInstantiator(registry).Instantiate(asset, graph, nullptr, &imports);
        if (!instantiated.Success)
        {
            for (const GraphInstantiationError& error : instantiated.Errors)
                diagnostics.push_back({ error.Message, error.PassName, error.SlotName, {} });
            return diagnostics;
        }

        const CompileResult compiled = GraphCompiler{}.Compile(graph.GetDescription());
        for (const CompileError& error : compiled.Errors)
            diagnostics.push_back({ error.Message, error.PassName, {}, error.ResourceName });
        return diagnostics;
    }
}
