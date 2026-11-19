#include "HedgehogInput/api/InputOverrides.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>

namespace HInput
{
    InputActionSet ApplyInputOverrides(const InputActionSet& project, const InputActionMap& overrides)
    {
        InputActionSet result = project;
        for (const InputAction& override : overrides.Actions)
        {
            if (const std::optional<size_t> index = FindAction(result.Game, override.Name))
            {
                result.Game.Actions[*index].Bindings = override.Bindings;
            }
            else
            {
                LOGWARNING("[Input] The input override '", override.Name,
                           "' names no action of the project; it is dropped.");
            }
        }
        return result;
    }

    bool SetActionBinding(InputActionMap&       overrides,
                          const InputActionMap& project,
                          std::string_view      action,
                          size_t                bindingIndex,
                          const InputBinding&   binding)
    {
        const std::optional<size_t> projectIndex = FindAction(project, action);
        if (!projectIndex || bindingIndex >= project.Actions[*projectIndex].Bindings.size())
        {
            return false;
        }

        std::optional<size_t> index = FindAction(overrides, action);
        if (!index)
        {
            overrides.Actions.push_back(project.Actions[*projectIndex]);
            index = overrides.Actions.size() - 1;
        }
        std::vector<InputBinding>& bindings = overrides.Actions[*index].Bindings;
        if (bindingIndex >= bindings.size())
        {
            return false;
        }
        bindings[bindingIndex] = binding;
        return true;
    }

    bool ResetActionBindings(InputActionMap& overrides, std::string_view action)
    {
        const auto found = std::find_if(overrides.Actions.begin(), overrides.Actions.end(),
                                        [&](const InputAction& candidate) { return candidate.Name == action; });
        if (found == overrides.Actions.end())
        {
            return false;
        }
        overrides.Actions.erase(found);
        return true;
    }
}
