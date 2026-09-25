#include "HedgehogRenderer/Graph/GraphInstantiator.hpp"

#include "HedgehogRenderer/Graph/GraphAssetVocabulary.hpp"
#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace Renderer
{
    namespace
    {
        using Errors = std::vector<GraphInstantiationError>;

        bool IsDepthFormat(RHI::Format format)
        {
            switch (format)
            {
                case RHI::Format::D16Unorm:
                case RHI::Format::D32Float:
                case RHI::Format::D24UnormS8Uint:
                case RHI::Format::D32FloatS8Uint:
                    return true;
                default:
                    return false;
            }
        }

        bool SameSizePolicy(const RGSizePolicy& lhs, const RGSizePolicy& rhs)
        {
            if (lhs.Kind != rhs.Kind)
                return false;
            if (lhs.Kind == RGSizePolicyKind::Absolute)
                return lhs.Width == rhs.Width && lhs.Height == rhs.Height;
            return lhs.Scale == rhs.Scale;
        }

        std::string OutputLabel(const GraphAssetOutput& output)
        {
            return "output slot " + std::to_string(output.Slot) + " ('" + output.Name + "')";
        }

        const char* KindName(PassParameterKind kind)
        {
            switch (kind)
            {
                case PassParameterKind::Text:       return "text";
                case PassParameterKind::Flag:       return "flag (true or false)";
                case PassParameterKind::Format:     return "format";
                case PassParameterKind::SizePolicy: return "size policy";
            }
            return "value";
        }

        bool IsValidParameter(PassParameterKind kind, const std::string& value)
        {
            switch (kind)
            {
                case PassParameterKind::Text:       return true;
                case PassParameterKind::Flag:       return ResolveFlag(value).has_value();
                case PassParameterKind::Format:     return ResolveFormat(value).has_value();
                case PassParameterKind::SizePolicy: return ResolveSizePolicy(value).has_value();
            }
            return false;
        }

        void ValidateOutputContract(const GraphAsset& asset, const std::vector<GraphOutputRequirement>& required,
                                    Errors& errors)
        {
            if (asset.Outputs.size() != required.size())
            {
                errors.push_back({ "graph declares " + std::to_string(asset.Outputs.size())
                                   + " output slot(s) but the view binds " + std::to_string(required.size()),
                                   "", "" });
            }
            const size_t count = std::min(asset.Outputs.size(), required.size());
            for (size_t i = 0; i < count; ++i)
            {
                const GraphAssetOutput& output = asset.Outputs[i];
                if (output.Format != required[i].Format)
                    errors.push_back({ OutputLabel(output) + ": format does not match the view's target", "", output.Name });
                if (!SameSizePolicy(output.Size, required[i].Size))
                    errors.push_back({ OutputLabel(output) + ": size policy does not match the view's target", "", output.Name });
            }
        }

        void ValidateImports(const GraphAsset& asset, const GraphDescription& description,
                             const GraphImports* imports, Errors& errors)
        {
            for (const GraphAssetImport& import : asset.Imports)
            {
                const auto supplied = imports ? imports->find(import.Name) : GraphImports::const_iterator{};
                if (!imports || supplied == imports->end())
                {
                    errors.push_back({ "import '" + import.Name + "' is not supplied by the caller", "", import.Name });
                    continue;
                }
                const RGResourceRecord* record = description.FindResource(supplied->second.Id);
                if (!record || record->IsBuffer)
                {
                    errors.push_back({ "import '" + import.Name + "' is supplied with a handle that is not a texture "
                                       "in this graph", "", import.Name });
                }
                else if (record->TextureDesc.Format != import.Format)
                {
                    errors.push_back({ "import '" + import.Name + "': format does not match the supplied texture '"
                                       + record->Name + "'", "", import.Name });
                }
            }
        }

        void ValidatePass(const GraphAssetPass& pass, const PassTypeInfo* info,
                          const std::unordered_set<std::string>& declaredNames, Errors& errors)
        {
            const std::string prefix = "pass '" + pass.Name + "': ";
            if (!info)
            {
                errors.push_back({ prefix + "unknown pass type '" + pass.Type + "'", pass.Name, "" });
                return;
            }

            for (const GraphAssetBinding& binding : pass.Bindings)
            {
                if (std::find(info->Slots.begin(), info->Slots.end(), binding.Slot) == info->Slots.end())
                {
                    errors.push_back({ prefix + "pass type '" + pass.Type + "' has no slot '" + binding.Slot + "'",
                                       pass.Name, binding.Slot });
                }
                else if (!declaredNames.contains(binding.Resource))
                {
                    errors.push_back({ prefix + "slot '" + binding.Slot + "' is bound to '" + binding.Resource
                                       + "', which is not a declared output, resource or import", pass.Name,
                                       binding.Slot });
                }
            }
            for (const std::string& slot : info->Slots)
            {
                const auto isBound = [&](const GraphAssetBinding& binding) { return binding.Slot == slot; };
                if (std::none_of(pass.Bindings.begin(), pass.Bindings.end(), isBound))
                {
                    errors.push_back({ prefix + "required slot '" + slot + "' of pass type '" + pass.Type
                                       + "' is not bound", pass.Name, slot });
                }
            }

            for (const GraphAssetParameter& parameter : pass.Parameters)
            {
                const auto isDeclared = [&](const PassParameterInfo& p) { return p.Name == parameter.Name; };
                const auto declared = std::find_if(info->Parameters.begin(), info->Parameters.end(), isDeclared);
                if (declared == info->Parameters.end())
                {
                    errors.push_back({ prefix + "pass type '" + pass.Type + "' has no parameter '" + parameter.Name
                                       + "'", pass.Name, "" });
                }
                else if (!IsValidParameter(declared->Kind, parameter.Value))
                {
                    errors.push_back({ prefix + "parameter '" + parameter.Name + "' value '" + parameter.Value
                                       + "' is not a valid " + KindName(declared->Kind), pass.Name, "" });
                }
            }
        }
    }

    RHI::TextureUsage DefaultTextureUsage(RHI::Format format)
    {
        return (IsDepthFormat(format) ? RHI::TextureUsage::DepthStencil : RHI::TextureUsage::ColorAttachment)
             | RHI::TextureUsage::Sampled;
    }

    GraphInstantiationResult GraphInstantiator::Validate(
        const GraphAsset& asset, const std::vector<GraphOutputRequirement>* requiredOutputs) const
    {
        GraphInstantiationResult result;
        if (requiredOutputs)
            ValidateOutputContract(asset, *requiredOutputs, result.Errors);

        std::unordered_set<std::string> declaredNames;
        for (const auto& output : asset.Outputs)
            declaredNames.insert(output.Name);
        for (const auto& resource : asset.Resources)
            declaredNames.insert(resource.Name);
        for (const auto& import : asset.Imports)
            declaredNames.insert(import.Name);

        for (const GraphAssetPass& pass : asset.Passes)
            ValidatePass(pass, m_Registry.Find(pass.Type), declaredNames, result.Errors);

        result.Success = result.Errors.empty();
        return result;
    }

    GraphInstantiationResult GraphInstantiator::Instantiate(
        const GraphAsset& asset, RenderGraphRuntime& graph,
        const std::vector<GraphOutputRequirement>* requiredOutputs, const GraphImports* imports) const
    {
        // 1. Validate everything that can be checked without declaring anything.
        GraphInstantiationResult result = Validate(asset, requiredOutputs);
        ValidateImports(asset, graph.GetDescription(), imports, result.Errors);
        result.Success = result.Errors.empty();
        if (!result.Success)
            return result;

        // 2. Declare, through the same public API a C++ caller uses.
        std::unordered_map<std::string, RGTexture> latest; // name -> newest version
        const auto declare = [&](const std::string& name, RHI::Format format, const RGSizePolicy& size)
        {
            latest[name] = graph.CreateTexture({ name, format, size, DefaultTextureUsage(format) });
        };
        for (const auto& output : asset.Outputs)
            declare(output.Name, output.Format, output.Size);
        for (const auto& resource : asset.Resources)
            declare(resource.Name, resource.Format, resource.Size);
        for (const auto& import : asset.Imports)
            latest[import.Name] = imports->at(import.Name);

        for (size_t i = 0; i < asset.Passes.size(); ++i)
        {
            const GraphAssetPass& pass = asset.Passes[i];
            PassInvocation invocation(pass.Name);
            for (const GraphAssetBinding& binding : pass.Bindings)
                invocation.SetSlot(binding.Slot, latest.at(binding.Resource));
            for (const GraphAssetParameter& parameter : pass.Parameters)
                invocation.SetParameter(parameter.Name, parameter.Value);

            m_Registry.Find(pass.Type)->Build(graph, invocation);

            // A write bumped the slot's version; later passes and outputs must see it.
            for (const GraphAssetBinding& binding : pass.Bindings)
                latest[binding.Resource] = invocation.GetSlot(binding.Slot);
        }

        // 3. Bind outputs. One no pass wrote stays unbound, so Compile() rejects the graph too.
        for (const auto& output : asset.Outputs)
        {
            const uint32_t  slot          = graph.AddOutputSlot(output.Name, output.Format, output.Size);
            const RGTexture latestVersion = latest.at(output.Name);
            if (latestVersion.Version == 0)
            {
                result.Errors.push_back({ OutputLabel(output) + " is never written by any pass", "", output.Name });
                continue;
            }
            graph.BindOutput(slot, latestVersion);
        }

        result.Success = result.Errors.empty();
        return result;
    }
}
