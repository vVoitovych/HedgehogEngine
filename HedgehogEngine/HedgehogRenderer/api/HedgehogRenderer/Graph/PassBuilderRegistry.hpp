#pragma once

#include "PassInvocation.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Renderer
{
    class RenderGraphRuntime;

    // A pass builder declares one pass instance into the graph (RENDERING.md section 6, Rule 1).
    // It owns its behaviour and its slot semantics; an asset only chooses which resources fill the
    // slots and what the parameters are. Called by GraphInstantiator and by C++ callers alike.
    using PassBuildFn = void (*)(RenderGraphRuntime& graph, PassInvocation& invocation);

    // How the instantiator checks a parameter string before any builder runs, using the
    // GraphAssetVocabulary resolvers. Text is accepted as-is (e.g. a shader override path).
    enum class PassParameterKind
    {
        Text,
        Flag,
        Format,
        SizePolicy,
    };

    struct PassParameterInfo
    {
        std::string       Name;
        PassParameterKind Kind = PassParameterKind::Text;
    };

    // What a pass type accepts. Every slot is required: an asset must bind each one, and may bind
    // nothing else. Parameters are optional; the builder supplies its own default when unset.
    struct PassTypeInfo
    {
        std::vector<std::string>       Slots;
        std::vector<PassParameterInfo> Parameters;
        PassBuildFn                    Build = nullptr;
    };

    class PassBuilderRegistry
    {
    public:
        // Returns false, leaving the existing registration in place, if type is already registered.
        [[nodiscard]] bool Register(std::string type, PassTypeInfo info);

        // nullptr if type is not registered.
        [[nodiscard]] const PassTypeInfo* Find(std::string_view type) const;

    private:
        std::unordered_map<std::string, PassTypeInfo> m_Types;
    };
}
