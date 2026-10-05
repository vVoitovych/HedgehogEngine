#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/System.hpp"

#include "HedgehogInput/api/ActionState.hpp"
#include "HedgehogInput/api/InputActionMap.hpp"
#include "HedgehogMath/api/Vector.hpp"
#include "HedgehogUI/api/UiHitTest.hpp"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace HedgehogEngine
{
    class EventBus;
    struct GameInputFrame;

    // The entity view over every UiCanvasComponent: the roots extraction walks to build the frame's
    // UI draw list, and the roots of the game UI's input handling, which runs in the Input phase
    // (EngineContext::UpdateGameInput runs it): UpdateInput while Playing, from the EventBus and
    // GameInputFrame services found in OnRegister (nothing without them), and ResetInput otherwise.
    class UiSystem : public ECS::System
    {
    public:
        HEDGEHOG_ENGINE_API void OnRegister(ECS::ECS& ecs) override;
        HEDGEHOG_ENGINE_API void OnUnregister(ECS::ECS& ecs) override;
        [[nodiscard]] HEDGEHOG_ENGINE_API ECS::SystemPhase GetPhase() const override;
        HEDGEHOG_ENGINE_API void OnFrame(ECS::ECS& ecs, const ECS::FrameContext& ctx) override;

        // The Game map actions the UI handles.
        static constexpr const char* POINTER_PRESS_ACTION  = "UiPointerPress";
        static constexpr const char* SUBMIT_ACTION         = "UiSubmit";
        static constexpr const char* NAVIGATE_UP_ACTION    = "UiNavigateUp";
        static constexpr const char* NAVIGATE_DOWN_ACTION  = "UiNavigateDown";
        static constexpr const char* NAVIGATE_LEFT_ACTION  = "UiNavigateLeft";
        static constexpr const char* NAVIGATE_RIGHT_ACTION = "UiNavigateRight";

        // One frame of UI input in Play mode, from the game's actions (map, evaluated into actions)
        // over a target of targetSize pixels, laid out as extraction lays it out. The pointer's
        // targets are the visible elements with an image or a button, the last drawn on top; the
        // topmost one under the pointer blocks the rest, and is hovered when it is an interactable
        // button. A press and a release of UiPointerPress on the same button clicks it (and gives it
        // focus); UiNavigate* moves focus to the nearest interactable button that way (the first one
        // when none has it) and UiSubmit clicks the focused one. Each click publishes
        // UiButtonClickedEvent on bus once the frame's states are written. Every button's State is set:
        // Pressed while pressed by the pointer (and under it) or held by UiSubmit with focus, Hovered
        // while under the pointer or focused, else Normal. While the pointer is over a target or a
        // press on a button is held, every action with a pointer binding (mouse button, pointer
        // delta, scroll) is consumed; a navigation or submit action is consumed when a button has
        // focus. Allocates nothing once its scratch lists have grown.
        HEDGEHOG_ENGINE_API void UpdateInput(ECS::ECS& ecs, const HInput::InputActionMap& map,
                                             HInput::ActionState& actions, const HM::Vector2& targetSize, EventBus& bus);

        // Outside Play: every button it styled back to Normal, and no hover, press or focus.
        HEDGEHOG_ENGINE_API void ResetInput(ECS::ECS& ecs);

        [[nodiscard]] HEDGEHOG_ENGINE_API std::optional<ECS::Entity> GetHovered(const ECS::ECS& ecs) const;
        [[nodiscard]] HEDGEHOG_ENGINE_API std::optional<ECS::Entity> GetFocused(const ECS::ECS& ecs) const;

    private:
        // An entity as it was when tracked, so a destroyed (and possibly recycled) one is not mistaken for it.
        struct TrackedEntity
        {
            ECS::Entity Entity     = 0;
            uint32_t    Generation = 0;
        };

        void CollectTargets(const ECS::ECS& ecs, const HM::Vector2& targetSize);
        std::optional<size_t> FindTarget(const ECS::ECS& ecs, const std::optional<TrackedEntity>& tracked) const;
        std::optional<TrackedEntity> Track(const ECS::ECS& ecs, std::optional<size_t> target) const;
        void WriteButtonStates(ECS::ECS& ecs, std::optional<size_t> hovered, std::optional<size_t> pressed,
                               std::optional<size_t> focused, bool submitDown);
        void ResetButtonStates(ECS::ECS& ecs);

    private:
        // This frame's targets in draw order, and their entities.
        std::vector<HUI::UiHitTarget> m_Targets;
        std::vector<ECS::Entity>      m_TargetEntities;
        // Enabled canvases as (SortOrder, entity), sorted into draw order.
        std::vector<std::pair<int32_t, ECS::Entity>> m_CanvasOrder;
        // Buttons whose State was set to other than Normal, to put back.
        std::vector<ECS::Entity> m_StyledButtons;
        std::vector<ECS::Entity> m_Clicks;

        std::optional<TrackedEntity> m_Hovered;
        std::optional<TrackedEntity> m_Pressed;
        std::optional<TrackedEntity> m_Focused;

        EventBus*             m_Bus   = nullptr;
        const GameInputFrame* m_Input = nullptr;
    };
}
