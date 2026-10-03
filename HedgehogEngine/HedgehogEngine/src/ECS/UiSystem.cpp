#include "HedgehogEngine/api/ECS/systems/UiSystem.hpp"

#include "HedgehogEngine/api/ECS/components/RenderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiButtonComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiCanvasComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiImageComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiRectComponent.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Events/UiEvents.hpp"

#include "ECS/api/components/Hierarchy.hpp"
#include "HedgehogSettings/api/LayerSettings.hpp"
#include "HedgehogUI/api/RectTransform.hpp"

#include <algorithm>
#include <array>

namespace HedgehogEngine
{
    namespace
    {
        // What every element of one canvas shares while its targets are collected.
        struct TargetCollector
        {
            const ECS::ECS&                Ecs;
            float                          Scale = 1.0f;
            std::vector<HUI::UiHitTarget>& Targets;
            std::vector<ECS::Entity>&      Entities;
        };

        bool OnEditorLayer(const ECS::ECS& ecs, ECS::Entity entity)
        {
            return ecs.HasComponent<RenderComponent>(entity) &&
                   ecs.GetComponent<RenderComponent>(entity).Layer == HedgehogSettings::LayerSettings::EDITOR_LAYER;
        }

        void CollectChildren(const TargetCollector& collector, ECS::Entity parent, const HX::UiRect& parentRect);

        // The element's target, then its children's: the order extraction draws them in.
        void CollectElement(const TargetCollector& collector, ECS::Entity entity, const HX::UiRect& parentRect)
        {
            const ECS::ECS& ecs = collector.Ecs;
            if (!ecs.HasComponent<UiRectComponent>(entity) || ecs.HasComponent<UiCanvasComponent>(entity))
                return;
            const UiRectComponent& rectComponent = ecs.GetComponent<UiRectComponent>(entity);
            if (!rectComponent.IsVisible || OnEditorLayer(ecs, entity))
                return;

            const HUI::RectTransform transform{ rectComponent.AnchorMin, rectComponent.AnchorMax, rectComponent.Pivot,
                                                rectComponent.Offset, rectComponent.Size };
            const HX::UiRect         rect     = HUI::ResolveRect(transform, parentRect);
            const bool               isButton = ecs.HasComponent<UiButtonComponent>(entity);
            if (isButton || ecs.HasComponent<UiImageComponent>(entity))
            {
                const bool interactable = isButton && ecs.GetComponent<UiButtonComponent>(entity).IsInteractable;
                collector.Targets.push_back({ HUI::ToPixels(rect, collector.Scale), interactable });
                collector.Entities.push_back(entity);
            }
            CollectChildren(collector, entity, rect);
        }

        void CollectChildren(const TargetCollector& collector, ECS::Entity parent, const HX::UiRect& parentRect)
        {
            if (!collector.Ecs.HasComponent<ECS::HierarchyComponent>(parent))
                return;
            for (const ECS::Entity child : collector.Ecs.GetComponent<ECS::HierarchyComponent>(parent).Children)
                CollectElement(collector, child, parentRect);
        }

        float CanvasScale(const UiCanvasComponent& canvas, const HM::Vector2& targetSize)
        {
            HUI::CanvasScaler scaler;
            scaler.Mode = canvas.ScaleMode == UiCanvasScaleMode::ScaleWithTargetSize
                              ? HUI::CanvasScaleMode::ScaleWithTargetSize
                              : HUI::CanvasScaleMode::ConstantPixelSize;
            scaler.ReferenceResolution = canvas.ReferenceResolution;
            scaler.MatchWidthOrHeight  = canvas.MatchWidthOrHeight;
            return HUI::ComputeCanvasScale(scaler, targetSize);
        }

        // Whether the action is driven by the pointer: a mouse button, the cursor's move or the scroll.
        bool HasPointerBinding(const HInput::InputAction& action)
        {
            return std::any_of(action.Bindings.begin(), action.Bindings.end(),
                               [](const HInput::InputBinding& binding)
                               {
                                   switch (binding.Source)
                                   {
                                   case HInput::BindingSource::MouseButton:
                                   case HInput::BindingSource::PointerDeltaX:
                                   case HInput::BindingSource::PointerDeltaY:
                                   case HInput::BindingSource::ScrollX:
                                   case HInput::BindingSource::ScrollY:
                                       return true;
                                   default:
                                       return false;
                                   }
                               });
        }
    }

    void UiSystem::UpdateInput(ECS::ECS& ecs, const HInput::InputActionMap& map, HInput::ActionState& actions,
                               const HM::Vector2& targetSize, EventBus& bus)
    {
        CollectTargets(ecs, targetSize);

        // What was tracked last frame and is still an interactable target.
        const auto interactable = [&](std::optional<size_t> target)
        { return target && m_Targets[*target].Interactable ? target : std::nullopt; };
        std::optional<size_t> pressed = interactable(FindTarget(ecs, m_Pressed));
        std::optional<size_t> focused = interactable(FindTarget(ecs, m_Focused));

        const HInput::PointerState& pointer = actions.Pointer;
        const std::optional<size_t> topmost =
            pointer.Inside ? HUI::FindTopmostTarget(m_Targets, pointer.Position) : std::nullopt;
        const std::optional<size_t> hovered = interactable(topmost);

        m_Clicks.clear();
        if (const std::optional<size_t> press = HInput::FindAction(map, POINTER_PRESS_ACTION))
        {
            if (HInput::WasActionPressed(actions, *press) && hovered)
            {
                pressed = hovered;
                focused = hovered;
            }
            else if (HInput::WasActionReleased(actions, *press))
            {
                if (pressed && pressed == hovered)
                    m_Clicks.push_back(m_TargetEntities[*pressed]);
                pressed.reset();
            }
            else if (!HInput::IsActionDown(actions, *press))
            {
                pressed.reset();
            }
        }
        else
        {
            pressed.reset();
        }

        constexpr std::array<std::pair<const char*, HUI::UiNavigateDirection>, 4> NAVIGATION = { {
            { NAVIGATE_UP_ACTION, HUI::UiNavigateDirection::Up },
            { NAVIGATE_DOWN_ACTION, HUI::UiNavigateDirection::Down },
            { NAVIGATE_LEFT_ACTION, HUI::UiNavigateDirection::Left },
            { NAVIGATE_RIGHT_ACTION, HUI::UiNavigateDirection::Right },
        } };
        for (const auto& [name, direction] : NAVIGATION)
        {
            const std::optional<size_t> action = HInput::FindAction(map, name);
            if (!action || !HInput::WasActionPressed(actions, *action))
                continue;
            const std::optional<size_t> next =
                focused ? HUI::FindNavigationTarget(m_Targets, *focused, direction) : HUI::FindFirstInteractable(m_Targets);
            if (next)
                focused = next;
            if (focused)
                HInput::ConsumeAction(actions, *action);
        }

        bool submitDown = false;
        if (const std::optional<size_t> submit = HInput::FindAction(map, SUBMIT_ACTION); submit && focused)
        {
            if (HInput::WasActionPressed(actions, *submit))
                m_Clicks.push_back(m_TargetEntities[*focused]);
            submitDown = HInput::IsActionDown(actions, *submit);
            HInput::ConsumeAction(actions, *submit);
        }

        if (topmost || pressed)
        {
            for (size_t action = 0; action < map.Actions.size(); ++action)
            {
                if (HasPointerBinding(map.Actions[action]))
                    HInput::ConsumeAction(actions, action);
            }
        }

        WriteButtonStates(ecs, hovered, pressed, focused, submitDown);
        m_Hovered = Track(ecs, hovered);
        m_Pressed = Track(ecs, pressed);
        m_Focused = Track(ecs, focused);

        // Last, so a handler that changes the scene finds this frame's states written.
        for (const ECS::Entity entity : m_Clicks)
            bus.Publish(UiButtonClickedEvent{ entity });
    }

    void UiSystem::ResetInput(ECS::ECS& ecs)
    {
        ResetButtonStates(ecs);
        m_Hovered.reset();
        m_Pressed.reset();
        m_Focused.reset();
    }

    std::optional<ECS::Entity> UiSystem::GetHovered(const ECS::ECS& ecs) const
    {
        if (m_Hovered && ecs.IsAlive(m_Hovered->Entity) && ecs.GetGeneration(m_Hovered->Entity) == m_Hovered->Generation)
            return m_Hovered->Entity;
        return std::nullopt;
    }

    std::optional<ECS::Entity> UiSystem::GetFocused(const ECS::ECS& ecs) const
    {
        if (m_Focused && ecs.IsAlive(m_Focused->Entity) && ecs.GetGeneration(m_Focused->Entity) == m_Focused->Generation)
            return m_Focused->Entity;
        return std::nullopt;
    }

    void UiSystem::CollectTargets(const ECS::ECS& ecs, const HM::Vector2& targetSize)
    {
        m_Targets.clear();
        m_TargetEntities.clear();
        if (!(targetSize.x() > 0.0f && targetSize.y() > 0.0f))
            return;

        m_CanvasOrder.clear();
        for (const ECS::Entity entity : GetEntities())
        {
            const UiCanvasComponent& canvas = ecs.GetComponent<UiCanvasComponent>(entity);
            if (canvas.IsEnabled && !OnEditorLayer(ecs, entity))
                m_CanvasOrder.emplace_back(canvas.SortOrder, entity);
        }
        std::sort(m_CanvasOrder.begin(), m_CanvasOrder.end());

        for (const auto& [sortOrder, entity] : m_CanvasOrder)
        {
            const float           scale = CanvasScale(ecs.GetComponent<UiCanvasComponent>(entity), targetSize);
            const TargetCollector collector{ ecs, scale, m_Targets, m_TargetEntities };
            CollectChildren(collector, entity, HUI::CanvasRect(targetSize, scale));
        }
    }

    std::optional<size_t> UiSystem::FindTarget(const ECS::ECS& ecs, const std::optional<TrackedEntity>& tracked) const
    {
        if (!tracked || !ecs.IsAlive(tracked->Entity) || ecs.GetGeneration(tracked->Entity) != tracked->Generation)
            return std::nullopt;
        const auto found = std::find(m_TargetEntities.begin(), m_TargetEntities.end(), tracked->Entity);
        if (found == m_TargetEntities.end())
            return std::nullopt;
        return static_cast<size_t>(found - m_TargetEntities.begin());
    }

    std::optional<UiSystem::TrackedEntity> UiSystem::Track(const ECS::ECS& ecs, std::optional<size_t> target) const
    {
        if (!target)
            return std::nullopt;
        const ECS::Entity entity = m_TargetEntities[*target];
        return TrackedEntity{ entity, ecs.GetGeneration(entity) };
    }

    void UiSystem::WriteButtonStates(ECS::ECS& ecs, std::optional<size_t> hovered, std::optional<size_t> pressed,
                                     std::optional<size_t> focused, bool submitDown)
    {
        ResetButtonStates(ecs);
        for (size_t target = 0; target < m_Targets.size(); ++target)
        {
            const ECS::Entity entity = m_TargetEntities[target];
            if (!m_Targets[target].Interactable || !ecs.HasComponent<UiButtonComponent>(entity))
                continue;

            const bool isPressed = (pressed == target && hovered == target) || (focused == target && submitDown);
            const bool isHovered = (hovered == target && (!pressed || pressed == target)) || focused == target;
            if (!isPressed && !isHovered)
                continue;
            ecs.GetComponent<UiButtonComponent>(entity).State = isPressed ? UiButtonState::Pressed : UiButtonState::Hovered;
            m_StyledButtons.push_back(entity);
        }
    }

    void UiSystem::ResetButtonStates(ECS::ECS& ecs)
    {
        for (const ECS::Entity entity : m_StyledButtons)
        {
            if (ecs.IsAlive(entity) && ecs.HasComponent<UiButtonComponent>(entity))
                ecs.GetComponent<UiButtonComponent>(entity).State = UiButtonState::Normal;
        }
        m_StyledButtons.clear();
    }
}
