#include "GraphEditing.hpp"

#include <algorithm>

namespace Editor::GraphEdit
{
    namespace
    {
        bool IsResourceNameTaken(const Renderer::GraphAsset& asset, std::string_view name)
        {
            return std::ranges::any_of(asset.Outputs, [&](const auto& entry) { return entry.Name == name; })
                || std::ranges::any_of(asset.Resources, [&](const auto& entry) { return entry.Name == name; })
                || std::ranges::any_of(asset.Imports, [&](const auto& entry) { return entry.Name == name; });
        }

        bool IsPassNameTaken(const Renderer::GraphAsset& asset, std::string_view name)
        {
            return std::ranges::any_of(asset.Passes, [&](const auto& pass) { return pass.Name == name; });
        }

        template<typename IsTaken>
        std::string UniqueName(std::string_view base, IsTaken&& isTaken)
        {
            const std::string root = base.empty() ? std::string("unnamed") : std::string(base);
            if (!isTaken(root))
                return root;
            for (uint32_t suffix = 1;; ++suffix)
            {
                std::string candidate = root + std::to_string(suffix);
                if (!isTaken(candidate))
                    return candidate;
            }
        }

        std::string& ResourceLikeName(Renderer::GraphAsset& asset, GraphNodeKind kind, size_t index)
        {
            switch (kind)
            {
            case GraphNodeKind::Output:   return asset.Outputs[index].Name;
            case GraphNodeKind::Import:   return asset.Imports[index].Name;
            default:                      return asset.Resources[index].Name;
            }
        }
    }

    std::string UniqueResourceName(const Renderer::GraphAsset& asset, std::string_view base)
    {
        return UniqueName(base, [&](std::string_view name) { return IsResourceNameTaken(asset, name); });
    }

    std::string UniquePassName(const Renderer::GraphAsset& asset, std::string_view base)
    {
        return UniqueName(base, [&](std::string_view name) { return IsPassNameTaken(asset, name); });
    }

    std::string AddPass(Renderer::GraphAsset& asset, std::string_view type)
    {
        Renderer::GraphAssetPass pass;
        pass.Type = std::string(type);
        pass.Name = UniquePassName(asset, type);
        asset.Passes.push_back(pass);
        return asset.Passes.back().Name;
    }

    std::string AddResourceLike(Renderer::GraphAsset& asset, GraphNodeKind kind, std::string_view base)
    {
        const std::string name = UniqueResourceName(asset, base);
        const Renderer::RGSizePolicy size = Renderer::RGSizePolicy::MakeRelativeToResult(1.0f);
        switch (kind)
        {
        case GraphNodeKind::Output:
            asset.Outputs.push_back({ static_cast<uint32_t>(asset.Outputs.size()), name,
                                      RHI::Format::R16G16B16A16Unorm, size });
            break;
        case GraphNodeKind::Import:
            asset.Imports.push_back({ name, RHI::Format::D32Float });
            break;
        default:
            asset.Resources.push_back({ name, RHI::Format::R16G16B16A16Unorm, size });
            break;
        }
        return name;
    }

    void RemovePass(Renderer::GraphAsset& asset, size_t index)
    {
        if (index < asset.Passes.size())
            asset.Passes.erase(asset.Passes.begin() + static_cast<std::ptrdiff_t>(index));
    }

    void RemoveResourceLike(Renderer::GraphAsset& asset, GraphNodeKind kind, size_t index)
    {
        std::string name;
        switch (kind)
        {
        case GraphNodeKind::Output:
            if (index >= asset.Outputs.size())
                return;
            name = asset.Outputs[index].Name;
            asset.Outputs.erase(asset.Outputs.begin() + static_cast<std::ptrdiff_t>(index));
            for (uint32_t slot = 0; slot < asset.Outputs.size(); ++slot)
                asset.Outputs[slot].Slot = slot; // slots stay contiguous from 0
            break;
        case GraphNodeKind::Import:
            if (index >= asset.Imports.size())
                return;
            name = asset.Imports[index].Name;
            asset.Imports.erase(asset.Imports.begin() + static_cast<std::ptrdiff_t>(index));
            break;
        default:
            if (index >= asset.Resources.size())
                return;
            name = asset.Resources[index].Name;
            asset.Resources.erase(asset.Resources.begin() + static_cast<std::ptrdiff_t>(index));
            break;
        }
        for (Renderer::GraphAssetPass& pass : asset.Passes)
            std::erase_if(pass.Bindings, [&](const auto& binding) { return binding.Resource == name; });
    }

    std::optional<std::string> RenamePass(Renderer::GraphAsset& asset, size_t index, std::string_view name)
    {
        if (index >= asset.Passes.size() || asset.Passes[index].Name == name)
            return std::nullopt;
        if (name.empty())
            return "A pass needs a name.";
        if (IsPassNameTaken(asset, name))
            return "Another pass is already called '" + std::string(name) + "'.";
        asset.Passes[index].Name = std::string(name);
        return std::nullopt;
    }

    std::optional<std::string> RenameResourceLike(Renderer::GraphAsset& asset, GraphNodeKind kind, size_t index,
                                                  std::string_view name)
    {
        std::string& current = ResourceLikeName(asset, kind, index);
        if (current == name)
            return std::nullopt;
        if (name.empty())
            return "A resource needs a name.";
        if (IsResourceNameTaken(asset, name))
            return "An output, resource or import is already called '" + std::string(name) + "'.";

        for (Renderer::GraphAssetPass& pass : asset.Passes)
        {
            for (Renderer::GraphAssetBinding& binding : pass.Bindings)
            {
                if (binding.Resource == current)
                    binding.Resource = std::string(name);
            }
        }
        current = std::string(name);
        return std::nullopt;
    }

    void BindSlot(Renderer::GraphAsset& asset, size_t passIndex, std::string_view slot, std::string_view resource)
    {
        if (passIndex >= asset.Passes.size())
            return;
        auto& bindings = asset.Passes[passIndex].Bindings;
        const auto it = std::ranges::find(bindings, slot, &Renderer::GraphAssetBinding::Slot);
        if (it != bindings.end())
            it->Resource = std::string(resource);
        else
            bindings.push_back({ std::string(slot), std::string(resource) });
    }

    void UnbindSlot(Renderer::GraphAsset& asset, size_t passIndex, std::string_view slot)
    {
        if (passIndex < asset.Passes.size())
            std::erase_if(asset.Passes[passIndex].Bindings, [&](const auto& binding) { return binding.Slot == slot; });
    }

    void SetParameter(Renderer::GraphAsset& asset, size_t passIndex, std::string_view name,
                      std::optional<std::string> value)
    {
        if (passIndex >= asset.Passes.size())
            return;
        auto& parameters = asset.Passes[passIndex].Parameters;
        const auto it = std::ranges::find(parameters, name, &Renderer::GraphAssetParameter::Name);
        if (!value)
        {
            if (it != parameters.end())
                parameters.erase(it);
        }
        else if (it != parameters.end())
        {
            it->Value = std::move(*value);
        }
        else
        {
            parameters.push_back({ std::string(name), std::move(*value) });
        }
    }
}
