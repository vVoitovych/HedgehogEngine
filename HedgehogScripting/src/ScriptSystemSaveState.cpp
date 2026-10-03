#include "HedgehogScripting/api/ScriptSystem.hpp"

#include "Bindings/ScriptHandles.hpp"

#include "HedgehogEngine/api/EngineContext.hpp"

#include "ECS/api/ECS.hpp"
#include "HedgehogMath/api/Quaternion.hpp"
#include "HedgehogMath/api/Vector.hpp"

#include "Logger/api/Logger.hpp"

#include "yaml-cpp/yaml.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <string>
#include <vector>

// Script state in save games: Lua values to YAML and back, plain data only, and the Scripts
// section built from (and restored into) the running scripts.
namespace HedgehogScripting
{
    namespace
    {
        constexpr const char* VECTOR3_TAG = "!vec3";
        constexpr const char* QUAT_TAG    = "!quat";
        constexpr const char* ENTITY_TAG  = "!entity";
        constexpr const char* STRING_TAG  = "tag:yaml.org,2002:str"; // written as !!str

        bool IsLuaInteger(const sol::object& value)
        {
            if (value.get_type() != sol::type::number)
                return false;
            lua_State* state = value.lua_state();
            value.push(state);
            const bool isInteger = lua_isinteger(state, -1) != 0;
            lua_pop(state, 1);
            return isInteger;
        }

        // Why a value cannot be saved, with where it is.
        struct SaveError : std::runtime_error
        {
            using std::runtime_error::runtime_error;
        };

        // Whether a plain scalar with this text would read back as something other than a string.
        bool ReadsAsNonString(const std::string& text)
        {
            if (text.empty() || text == "~" || text == "null" || text == "true" || text == "false")
                return true;
            double     number = 0.0;
            const auto result = std::from_chars(text.data(), text.data() + text.size(), number);
            return result.ec == std::errc() && result.ptr == text.data() + text.size();
        }

        YAML::Node StringNode(const std::string& text)
        {
            YAML::Node node(text);
            if (ReadsAsNonString(text))
                node.SetTag(STRING_TAG);
            return node;
        }

        // A Lua value as a scalar or a table key's text form, by the scalar rules (see FromScalar).
        sol::object FromScalar(sol::state_view lua, const YAML::Node& node)
        {
            const std::string& text = node.Scalar();
            if (node.Tag() == STRING_TAG || node.Tag() == "!")
                return sol::make_object(lua, text);
            if (text == "true" || text == "false")
                return sol::make_object(lua, text == "true");
            if (text == "~" || text == "null" || text.empty())
                return sol::lua_nil;

            int64_t integer = 0;
            const auto asInteger = std::from_chars(text.data(), text.data() + text.size(), integer);
            if (asInteger.ec == std::errc() && asInteger.ptr == text.data() + text.size())
                return sol::make_object(lua, integer);
            double number = 0.0;
            if (YAML::convert<double>::decode(node, number))
                return sol::make_object(lua, number);
            return sol::make_object(lua, text);
        }

        class LuaToYaml
        {
        public:
            explicit LuaToYaml(const ECS::ECS& ecs) : m_Ecs(ecs) {}

            YAML::Node Convert(const sol::object& value, const std::string& where, int depth)
            {
                switch (value.get_type())
                {
                case sol::type::lua_nil:
                case sol::type::none:
                    return YAML::Node(YAML::NodeType::Null);
                case sol::type::boolean:
                    return YAML::Node(value.as<bool>());
                case sol::type::number:
                    return Number(value);
                case sol::type::string:
                    return StringNode(value.as<std::string>());
                case sol::type::table:
                    return Table(value.as<sol::table>(), where, depth);
                case sol::type::userdata:
                    return Userdata(value, where);
                default:
                    throw SaveError("cannot save " + where + ": it is a " + sol::type_name(value.lua_state(), value.get_type()));
                }
            }

        private:
            static YAML::Node Number(const sol::object& value)
            {
                return IsLuaInteger(value) ? YAML::Node(value.as<int64_t>()) : YAML::Node(value.as<double>());
            }

            YAML::Node Userdata(const sol::object& value, const std::string& where) const
            {
                YAML::Node node;
                if (value.is<HM::Vector3>())
                {
                    const HM::Vector3 v = value.as<HM::Vector3>();
                    node.push_back(v.x());
                    node.push_back(v.y());
                    node.push_back(v.z());
                    node.SetTag(VECTOR3_TAG);
                }
                else if (value.is<HM::Quaternion>())
                {
                    const HM::Quaternion q = value.as<HM::Quaternion>();
                    node.push_back(q.x());
                    node.push_back(q.y());
                    node.push_back(q.z());
                    node.push_back(q.w());
                    node.SetTag(QUAT_TAG);
                }
                else if (value.is<Bindings::ScriptEntity>())
                {
                    const Bindings::ScriptEntity entity = value.as<Bindings::ScriptEntity>();
                    node = Bindings::IsValid(m_Ecs, entity) ? entity.Id : ECS::INVALID_ENTITY;
                    node.SetTag(ENTITY_TAG);
                }
                else
                {
                    throw SaveError("cannot save " + where + ": it is a userdata that is not a Vector3, Quat or Entity");
                }
                node.SetStyle(YAML::EmitterStyle::Flow);
                return node;
            }

            YAML::Node Table(const sol::table& table, const std::string& where, int depth)
            {
                if (depth > ScriptSystem::MAX_SAVE_DEPTH)
                    throw SaveError("cannot save " + where + ": tables nest deeper than " +
                                    std::to_string(ScriptSystem::MAX_SAVE_DEPTH));
                const void* identity = table.pointer();
                if (std::find(m_Open.begin(), m_Open.end(), identity) != m_Open.end())
                    throw SaveError("cannot save " + where + ": the table contains itself");
                m_Open.push_back(identity);

                // 1..n with no other key is a sequence; anything else a map.
                const size_t length = table.size();
                size_t       count  = 0;
                bool         isSequence = true;
                for (const auto& [key, value] : table)
                {
                    ++count;
                    if (!IsLuaInteger(key) || key.as<int64_t>() < 1 || static_cast<size_t>(key.as<int64_t>()) > length)
                        isSequence = false;
                }
                isSequence = isSequence && count == length && length > 0;

                YAML::Node node(isSequence ? YAML::NodeType::Sequence : YAML::NodeType::Map);
                if (isSequence)
                {
                    for (size_t i = 1; i <= length; ++i)
                        node.push_back(Convert(table.get<sol::object>(i), where + "[" + std::to_string(i) + "]", depth + 1));
                }
                else
                {
                    for (const auto& [key, value] : table)
                    {
                        if (key.get_type() == sol::type::string)
                        {
                            const std::string name = key.as<std::string>();
                            node[StringNode(name)] = Convert(value, where + "." + name, depth + 1);
                        }
                        else if (IsLuaInteger(key))
                        {
                            const int64_t index = key.as<int64_t>();
                            node[YAML::Node(index)] = Convert(value, where + "[" + std::to_string(index) + "]", depth + 1);
                        }
                        else
                        {
                            throw SaveError("cannot save " + where + ": it has a key that is not a string or an integer");
                        }
                    }
                }
                m_Open.pop_back();
                return node;
            }

            const ECS::ECS&          m_Ecs;
            std::vector<const void*> m_Open; // the tables on the path being converted
        };

        sol::object ToLua(sol::state_view lua, const ECS::ECS& ecs, const YAML::Node& node, int depth)
        {
            if (depth > ScriptSystem::MAX_SAVE_DEPTH)
                throw std::runtime_error("the saved value nests deeper than " + std::to_string(ScriptSystem::MAX_SAVE_DEPTH));

            if (node.Tag() == VECTOR3_TAG)
            {
                if (!node.IsSequence() || node.size() != 3)
                    throw std::runtime_error("a !vec3 needs 3 numbers");
                return sol::make_object(lua, HM::Vector3(node[0].as<float>(), node[1].as<float>(), node[2].as<float>()));
            }
            if (node.Tag() == QUAT_TAG)
            {
                if (!node.IsSequence() || node.size() != 4)
                    throw std::runtime_error("a !quat needs 4 numbers");
                return sol::make_object(lua, HM::Quaternion(node[0].as<float>(), node[1].as<float>(), node[2].as<float>(),
                                                            node[3].as<float>()));
            }
            if (node.Tag() == ENTITY_TAG)
            {
                const ECS::Entity id = node.as<ECS::Entity>();
                const bool alive = id != ECS::INVALID_ENTITY && ecs.IsAlive(id);
                return sol::make_object(lua, alive ? Bindings::MakeScriptEntity(ecs, id) : Bindings::ScriptEntity{});
            }

            switch (node.Type())
            {
            case YAML::NodeType::Null:
                return sol::lua_nil;
            case YAML::NodeType::Scalar:
                return FromScalar(lua, node);
            case YAML::NodeType::Sequence:
            {
                sol::table table = lua.create_table(static_cast<int>(node.size()), 0);
                for (size_t i = 0; i < node.size(); ++i)
                    table[i + 1] = ToLua(lua, ecs, node[i], depth + 1);
                return table;
            }
            case YAML::NodeType::Map:
            {
                sol::table table = lua.create_table();
                for (const auto& entry : node)
                {
                    if (!entry.first.IsScalar())
                        throw std::runtime_error("a saved table key is not a string or an integer");
                    table[FromScalar(lua, entry.first)] = ToLua(lua, ecs, entry.second, depth + 1);
                }
                return table;
            }
            default:
                throw std::runtime_error("the saved value is not valid YAML");
            }
        }
    }

    YAML::Node ScriptSystem::SaveScriptState(ECS::ECS& ecs)
    {
        std::vector<ECS::Entity> entities;
        for (const auto& [entity, script] : m_Scripts)
            entities.push_back(entity);
        std::sort(entities.begin(), entities.end());

        YAML::Node section(YAML::NodeType::Map);
        for (const ECS::Entity entity : entities)
        {
            const auto it = m_Scripts.find(entity);
            if (it == m_Scripts.end() || it->second.Faulted || !it->second.Self.valid())
                continue;

            // OnSave may change the scripts; copy what is needed first.
            const std::string scriptPath = it->second.ScriptPath;
            const std::string entityName = it->second.EntityName;
            const sol::table  self       = it->second.Self;
            const sol::table  environment = it->second.Environment;

            try
            {
                LuaToYaml  convert(ecs);
                YAML::Node entry(YAML::NodeType::Map);
                entry["Script"] = scriptPath;

                YAML::Node properties(YAML::NodeType::Map);
                if (const auto scriptClass = m_Classes.find(scriptPath); scriptClass != m_Classes.end())
                {
                    for (const ScriptPropertyDeclaration& declaration : scriptClass->second.Declarations)
                    {
                        const std::string& name = declaration.Default.Name;
                        properties[name] = convert.Convert(self.get<sol::object>(name), "self." + name, 1);
                    }
                }
                entry["Properties"] = properties;

                m_RunningEntity = entity;
                const sol::protected_function_result saved = m_Invoke(environment, self, "OnSave");
                m_RunningEntity.reset();
                if (!saved.valid())
                {
                    const sol::error error = saved;
                    throw SaveError(std::string("OnSave failed: ") + error.what());
                }
                if (saved.return_count() > 0 && saved.get_type() != sol::type::lua_nil)
                {
                    if (saved.get_type() != sol::type::table)
                        throw SaveError("OnSave must return a table or nil");
                    entry["State"] = convert.Convert(saved.get<sol::object>(), "OnSave()", 1);
                }
                section[YAML::Node(entity)] = entry;
            }
            catch (const std::exception& e)
            {
                m_RunningEntity.reset();
                LogError(entityName, scriptPath, std::string("not saved: ") + e.what());
            }
        }
        return section;
    }

    void ScriptSystem::LoadScriptState(ECS::ECS& ecs, const YAML::Node& section)
    {
        if (!section.IsMap())
        {
            if (!section.IsNull())
                LOGWARNING("[Script] The saved script state is not a map; nothing is loaded.");
            return;
        }

        for (const auto& item : section)
        {
            ECS::Entity entity = ECS::INVALID_ENTITY;
            PendingLoad load;
            try
            {
                entity           = item.first.as<ECS::Entity>();
                const YAML::Node entry = item.second;
                load.ScriptPath  = NormalizeScriptPath(entry["Script"].as<std::string>());
                load.Properties  = m_Lua.create_table();
                if (const YAML::Node properties = entry["Properties"])
                {
                    for (const auto& property : properties)
                        load.Properties[property.first.as<std::string>()] = ToLua(m_Lua, ecs, property.second, 1);
                }
                load.State = entry["State"] ? ToLua(m_Lua, ecs, entry["State"], 1) : sol::make_object(m_Lua, sol::lua_nil);
            }
            catch (const std::exception& e)
            {
                LOGWARNING("[Script] The saved state of entity " + YAML::Dump(item.first) + " is skipped: " + e.what() + ".");
                continue;
            }

            const auto running = m_Scripts.find(entity);
            if (running == m_Scripts.end())
            {
                m_PendingLoads[entity] = std::move(load);
                continue;
            }
            if (running->second.ScriptPath != load.ScriptPath || !running->second.Self.valid())
            {
                LOGWARNING("[Script] " + running->second.EntityName + " (" + running->second.ScriptPath +
                           "): the saved state is for " + load.ScriptPath + "; it is not loaded.");
                continue;
            }
            if (!ApplyLoad(entity, running->second, load))
            {
                if (const auto failed = m_Scripts.find(entity); failed != m_Scripts.end())
                    failed->second.Faulted = true;
            }
        }
    }

    bool ScriptSystem::ApplyLoad(ECS::Entity entity, EntityScript& script, const PendingLoad& load)
    {
        if (const auto scriptClass = m_Classes.find(script.ScriptPath); scriptClass != m_Classes.end())
        {
            for (const ScriptPropertyDeclaration& declaration : scriptClass->second.Declarations)
            {
                const std::string&  name  = declaration.Default.Name;
                const sol::object   value = load.Properties.get<sol::object>(name);
                if (value.valid() && value.get_type() != sol::type::lua_nil)
                    script.Self[name] = value;
            }
        }
        script.Started = true; // a loaded script carries on: no OnStart
        return Invoke(entity, script, "OnLoad", load.State);
    }
}
