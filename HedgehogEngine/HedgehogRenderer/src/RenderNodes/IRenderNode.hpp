#pragma once

#include "RenderGraph/RenderGraphTypes.hpp"

#include <cstdint>
#include <vector>

namespace HedgehogEngine
{
    struct FrameData;
}

namespace RHI
{
    class IRHIDevice;
}

namespace HedgehogSettings
{
    class Settings;
}

namespace Renderer
{
    class ResourceManager;
    class RenderContext;
    class RenderGraph;

    class IRenderNode
    {
    public:
        virtual ~IRenderNode() = default;

        // Called by RenderGraph::Compile(). Override to declare resource slots
        // (read/write dependencies) that future phases use for barrier planning.
        virtual void Setup(RenderGraph& graph) {}

        virtual void Execute(RenderContext& ctx)       = 0;
        virtual void Cleanup(RHI::IRHIDevice& device)  = 0;

        virtual const RenderNodeDesc& GetDesc() const { return m_Desc; }

        virtual void OnWindowResize(RHI::IRHIDevice& device,
                                    const ResourceManager& resourceManager) {}
        virtual void OnSceneViewResize(RHI::IRHIDevice& device,
                                       const ResourceManager& resourceManager) {}
        virtual void OnSettingsChanged(RHI::IRHIDevice& device,
                                       const HedgehogSettings::Settings& settings,
                                       const ResourceManager& resourceManager) {}
        virtual void UpdateData(const HedgehogEngine::FrameData& frame,
                                uint32_t frameIndex,
                                const HedgehogSettings::Settings& settings) {}

        virtual void  BeginFrame()                  {}
        virtual void  DiscardFrame()                {}
        virtual void* GetSceneViewTextureId() const { return nullptr; }

        bool IsEnabled() const  { return m_Enabled; }
        void SetEnabled(bool v) { m_Enabled = v; }

        // Called by RenderNodeManager::LoadGraphPreset() when a node entry contains
        // "inputs" or "outputs" blocks. These override whatever Setup() declares in
        // m_Desc, allowing YAML to augment or replace C++-declared slot bindings.
        void SetYAMLInputs(std::vector<ResourceSlotDesc> inputs)
        {
            m_YAMLInputs    = std::move(inputs);
            m_HasYAMLInputs = true;
        }
        void SetYAMLOutputs(std::vector<ResourceSlotDesc> outputs)
        {
            m_YAMLOutputs    = std::move(outputs);
            m_HasYAMLOutputs = true;
        }

        // Called by RenderGraph::Compile() immediately after Setup() to apply any
        // pending YAML binding overrides to m_Desc.
        void ApplyYAMLBindings()
        {
            if (m_HasYAMLInputs)
            {
                m_Desc.inputs   = std::move(m_YAMLInputs);
                m_HasYAMLInputs = false;
            }
            if (m_HasYAMLOutputs)
            {
                m_Desc.outputs   = std::move(m_YAMLOutputs);
                m_HasYAMLOutputs = false;
            }
        }

    protected:
        RenderNodeDesc m_Desc;

    private:
        bool                          m_Enabled        = true;
        std::vector<ResourceSlotDesc> m_YAMLInputs;
        std::vector<ResourceSlotDesc> m_YAMLOutputs;
        bool                          m_HasYAMLInputs  = false;
        bool                          m_HasYAMLOutputs = false;
    };
}
