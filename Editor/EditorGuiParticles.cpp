#include "EditorGui.hpp"

#include "Tools/ParticleEditor/ParticleEditorWindow.hpp"
#include "Widgets/PropertyFields.hpp"

#include "HedgehogEngine/api/ECS/components/ParticleSystemComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/ParticleSystem.hpp"
#include "HedgehogEngine/api/Engine.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"

#include "EcsSerialization/api/ComponentTypeRegistry.hpp"
#include "ECS/api/ECS.hpp"

#include "imgui.h"

namespace Editor
{
    void EditorGui::DrawParticleSystemComponent(HedgehogEngine::Engine& context)
    {
        auto&             engineContext = context.GetEngineContext();
        auto&             ecs           = engineContext.GetECS();
        const ECS::Entity entity        = m_SelectedEntity.value();
        const auto*       info          = engineContext.GetComponentTypes().Find("ParticleSystemComponent");
        // The header, its Enabled checkbox and the reflected rows, Effect taking a dropped .particle.
        if (!info || !DrawRegisteredComponent(context, *info) || !ecs.HasComponent<HedgehogEngine::ParticleSystemComponent>(entity))
            return;
        const std::string& effect = ecs.GetComponent<HedgehogEngine::ParticleSystemComponent>(entity).Effect;
        if (!effect.empty() && ImGui::Button("Edit effect"))
            m_ParticleEditorWindow->OpenEffect(effect);
        if (engineContext.GetPlayState() != HedgehogEngine::PlayState::Edit || !BeginPropertyTable("##ParticlePreview"))
            return;

        // The preview runs the selected instance in Edit mode; Play mode, or selecting another entity, ends it.
        HedgehogEngine::ParticleSystem& particles  = *engineContext.GetParticleSystem();
        const bool                      previewing = particles.GetPreview() == entity && particles.IsPlaying(ecs, entity);
        PropertyLabel("Preview");
        if (ImGui::Button(previewing ? "Restart" : "Play"))
        {
            particles.SetPreview(ecs, entity);
            particles.Play(ecs, entity);
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(particles.GetPreview() != entity);
        if (ImGui::Button("Stop"))
            particles.SetPreview(ecs, ECS::INVALID_ENTITY);
        ImGui::EndDisabled();
        EndPropertyTable();
    }
}
