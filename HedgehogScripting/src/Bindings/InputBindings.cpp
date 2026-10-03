#include "Bindings.hpp"

#include "HedgehogEngine/api/EngineContext.hpp"

#include "HedgehogInput/api/ActionState.hpp"
#include "HedgehogInput/api/InputActionMap.hpp"

#include <stdexcept>
#include <string>
#include <tuple>

namespace HedgehogScripting::Bindings
{
    namespace
    {
        // The index of the game action called name; an unknown name is a script error.
        size_t ActionIndex(const HedgehogEngine::EngineContext& context, const std::string& name)
        {
            const std::optional<size_t> index = HInput::FindAction(context.GetInputActions().Game, name);
            if (!index)
                throw std::runtime_error("Input: no action '" + name + "' in " +
                                         HedgehogEngine::EngineContext::INPUT_ACTIONS_PATH);
            return *index;
        }
    }

    void RegisterInput(sol::state& lua, HedgehogEngine::EngineContext& context)
    {
        // Every function reads the engine's game action state when called, so a reloaded actions
        // file or the next frame's input is seen at once.
        sol::table input = lua.create_named_table("Input");

        input.set_function("isDown", [&context](const std::string& action)
                           { return HInput::IsActionDown(context.GetGameActionState(), ActionIndex(context, action)); });
        input.set_function("wasPressed", [&context](const std::string& action)
                           { return HInput::WasActionPressed(context.GetGameActionState(), ActionIndex(context, action)); });
        input.set_function("wasReleased", [&context](const std::string& action)
                           { return HInput::WasActionReleased(context.GetGameActionState(), ActionIndex(context, action)); });
        input.set_function("value", [&context](const std::string& action)
                           { return HInput::GetActionValue(context.GetGameActionState(), ActionIndex(context, action)); });
        input.set_function("consume", [&context](const std::string& action)
                           { HInput::ConsumeAction(context.GetGameActionState(), ActionIndex(context, action)); });

        input.set_function("pointerPosition", [&context]()
        {
            const HM::Vector2& position = context.GetGameActionState().Pointer.Position;
            return std::make_tuple(position.x(), position.y());
        });
        input.set_function("pointerDelta", [&context]()
        {
            const HM::Vector2& delta = context.GetGameActionState().Pointer.Delta;
            return std::make_tuple(delta.x(), delta.y());
        });
        input.set_function("isPointerInside", [&context]() { return context.GetGameActionState().Pointer.Inside; });
    }
}
