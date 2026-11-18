#include "Bindings.hpp"

#include "HedgehogEngine/api/EngineContext.hpp"

#include "Logger/api/Logger.hpp"

#include <memory>
#include <stdexcept>
#include <string>

namespace HedgehogScripting::Bindings
{
    void RegisterApplication(sol::state& lua, HedgehogEngine::EngineContext& context)
    {
        sol::table app  = lua.create_named_table("Application");
        sol::table meta = lua.create_table();

        // Outside Play (a script's top level also runs when the editor describes it) nothing is
        // asked, and the first call says why.
        auto warned = std::make_shared<bool>(false);
        meta.set_function("quit", [&context, warned]()
        {
            if (context.GetPlayState() == HedgehogEngine::PlayState::Edit)
            {
                if (!*warned)
                {
                    LOGWARNING("[Script] The application quits only in Play mode; Application.quit did nothing.");
                }
                *warned = true;
                return;
            }
            context.GetApplicationRequests().QuitRequested = true;
        });

        // The table stays empty, so every read reaches __index and every write __newindex: no
        // script can replace quit for the others. quit lives in the metatable, read from there so
        // no closure holds a reference into the state.
        meta[sol::meta_function::index] = [&context](sol::table self, const sol::object& key,
                                                     sol::this_state state) -> sol::object
        {
            if (!key.is<std::string>())
            {
                return sol::lua_nil;
            }
            const std::string name = key.as<std::string>();
            if (name == "quit")
            {
                return self[sol::metatable_key].get<sol::table>().raw_get<sol::object>("quit");
            }
            if (name == "isEditor")
            {
                return sol::make_object(state, context.GetApplicationRequests().IsEditor);
            }
            return sol::lua_nil;
        };
        meta[sol::meta_function::new_index] = [](sol::table, const sol::object& key, sol::object)
        {
            const std::string name = key.is<std::string>() ? key.as<std::string>() : "<key>";
            throw std::runtime_error("Application." + name + " is read-only");
        };
        app[sol::metatable_key] = meta;
    }
}
