#include "Bindings.hpp"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Save/SaveGameManager.hpp"

#include "Logger/api/Logger.hpp"

#include <memory>
#include <stdexcept>
#include <string>

namespace HedgehogScripting::Bindings
{
    namespace
    {
        void RequireSlotName(const std::string& slot)
        {
            if (!HedgehogEngine::SaveGameManager::IsValidSlotName(slot))
                throw std::runtime_error("'" + slot + "' is not a save slot name (1 to 64 of A-Z, a-z, 0-9, '_' and '-')");
        }

        // Saves change only in Play mode (a script's top level also runs when the editor describes
        // it): outside it a call does nothing and the first one logs why.
        struct PlayGuard
        {
            HedgehogEngine::EngineContext& Context;
            bool                           Warned = false;

            bool Allows(const char* call)
            {
                if (Context.GetPlayState() != HedgehogEngine::PlayState::Edit)
                    return true;
                if (!Warned)
                    LOGWARNING("[Script] Saves change only in Play mode;", std::string(call), "did nothing.");
                Warned = true;
                return false;
            }
        };
    }

    void RegisterSave(sol::state& lua, HedgehogEngine::EngineContext& context)
    {
        HedgehogEngine::SaveGameManager& saves = context.GetSaveGames();
        auto                             guard = std::make_shared<PlayGuard>(PlayGuard{ context });
        sol::table                       table = lua.create_named_table("Save");

        table["write"] = [&saves, guard](const std::string& slot)
        {
            RequireSlotName(slot);
            return guard->Allows("Save.write()") && saves.RequestSave(slot);
        };
        table["load"] = [&saves, guard](const std::string& slot)
        {
            RequireSlotName(slot);
            return guard->Allows("Save.load()") && saves.RequestLoad(slot);
        };
        table["delete"] = [&saves, guard](const std::string& slot)
        {
            RequireSlotName(slot);
            return guard->Allows("Save.delete()") && saves.DeleteSlot(slot);
        };
        table["exists"] = [&saves](const std::string& slot)
        {
            RequireSlotName(slot);
            return saves.SlotExists(slot);
        };
        table["list"] = [&saves](sol::this_state state)
        {
            sol::state_view lua(state);
            sol::table      slots = lua.create_table();
            for (const EcsSerialization::SaveSlotInfo& info : saves.ListSlots())
            {
                const std::string reason = saves.GetUnloadableReason(info.Metadata);
                sol::table        slot   = lua.create_table_with(
                    "name", info.Name, "scene", info.Metadata.ScenePath, "timestamp", info.Metadata.Timestamp, "playTime",
                    info.Metadata.PlayTime, "gameDataVersion", info.Metadata.GameDataVersion, "saveVersion",
                    info.Metadata.SaveVersion, "loadable", reason.empty());
                if (!reason.empty())
                    slot["reason"] = reason;
                slots.add(slot);
            }
            return slots;
        };
    }
}
