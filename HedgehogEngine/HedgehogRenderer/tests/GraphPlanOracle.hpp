#pragma once

#include "HedgehogRenderer/Graph/CompiledGraph.hpp"
#include "HedgehogRenderer/Graph/GraphCompiler.hpp"
#include "HedgehogRenderer/Graph/GraphDescription.hpp"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

// The C++ equivalence oracle (RENDERING.md section 6, Rule 2): two graphs are equivalent when
// their compiled plans are identical — same texture declarations, same surviving passes in the
// same order, same barriers, same lifetimes. The plan is rendered as canonical text keyed by
// resource *name*, never RGResourceId, so a hand-written twin need not declare resources in the
// asset's order. Comparing two strings also makes doctest print both plans on a mismatch.
namespace RGTest
{
    inline std::string ResourceName(const Renderer::GraphDescription& description, Renderer::RGResourceId id)
    {
        const Renderer::RGResourceRecord* record = description.FindResource(id);
        return record ? record->Name : "<unknown " + std::to_string(id) + ">";
    }

    // Returns the plan text, or "compile failed: ..." so a failure is itself a visible mismatch.
    inline std::string DescribeCompiledPlan(const Renderer::GraphDescription& description)
    {
        const Renderer::CompileResult result = Renderer::GraphCompiler{}.Compile(description);
        std::ostringstream out;
        if (!result.Success)
        {
            out << "compile failed:";
            for (const auto& error : result.Errors)
                out << "\n  " << error.Message;
            return out.str();
        }

        // Texture declarations, in name order so declaration order does not matter.
        std::vector<std::string> textures;
        for (const auto& resource : description.Resources)
        {
            const Renderer::RGTextureDesc& desc = resource.TextureDesc;
            std::ostringstream line;
            line << "texture " << resource.Name << " format=" << static_cast<uint32_t>(desc.Format)
                 << " usage=" << static_cast<uint32_t>(desc.Usage) << " size=" << static_cast<int>(desc.Size.Kind)
                 << ':' << desc.Size.Width << 'x' << desc.Size.Height << '@' << desc.Size.Scale
                 << (resource.IsImported ? " imported" : "");
            textures.push_back(line.str());
        }
        std::sort(textures.begin(), textures.end());
        for (const auto& line : textures)
            out << line << '\n';

        for (const auto& pass : result.Graph.Passes)
        {
            out << "pass " << pass.Name << '\n';
            for (const auto& barrier : pass.TextureBarriers)
            {
                out << "  barrier " << ResourceName(description, barrier.Id) << ' '
                    << static_cast<int>(barrier.Before) << "->" << static_cast<int>(barrier.After)
                    << " mips " << barrier.Range.BaseMipLevel << '+' << barrier.Range.MipLevelCount
                    << " layers " << barrier.Range.BaseArrayLayer << '+' << barrier.Range.ArrayLayerCount << '\n';
            }
        }

        std::vector<std::string> lifetimes;
        for (const auto& lifetime : result.Graph.ResourceLifetimes)
        {
            lifetimes.push_back("lifetime " + ResourceName(description, lifetime.Id) + ' '
                                + std::to_string(lifetime.FirstUsePass) + ".." + std::to_string(lifetime.LastUsePass));
        }
        std::sort(lifetimes.begin(), lifetimes.end());
        for (const auto& line : lifetimes)
            out << line << '\n';
        return out.str();
    }
}
