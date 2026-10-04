#include "VariableStore.hpp"

extern "C"
{
#include "lauxlib.h"
#include "lua.h"
}

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string_view>

namespace LuaDebug
{
    namespace
    {
        constexpr size_t MAX_STRING_SHOWN = 200;

        // A table key in a field path: a name or string, or a number.
        struct PathKey
        {
            bool        IsNumber = false;
            lua_Integer Number   = 0;
            std::string Text;
        };

        struct FieldPath
        {
            std::string          Root;
            std::vector<PathKey> Keys;
        };

        // "a.b[3]['c']": names, dotted names, and integer or quoted-string subscripts, nothing else.
        std::optional<FieldPath> ParsePath(const std::string& expression)
        {
            const auto isNameStart = [](char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; };
            const auto isNameChar  = [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; };
            size_t     i           = 0;
            const auto readName    = [&]() -> std::optional<std::string>
            {
                if (i >= expression.size() || !isNameStart(expression[i]))
                    return std::nullopt;
                const size_t start = i;
                while (i < expression.size() && isNameChar(expression[i]))
                    ++i;
                return expression.substr(start, i - start);
            };

            FieldPath                        path;
            const std::optional<std::string> root = readName();
            if (!root)
                return std::nullopt;
            path.Root = *root;
            while (i < expression.size())
            {
                PathKey key;
                if (expression[i] == '.')
                {
                    ++i;
                    const std::optional<std::string> name = readName();
                    if (!name)
                        return std::nullopt;
                    key.Text = *name;
                }
                else if (expression[i] == '[' && i + 1 < expression.size())
                {
                    ++i;
                    const char quote = expression[i];
                    if (quote == '"' || quote == '\'')
                    {
                        const size_t end = expression.find(quote, i + 1);
                        if (end == std::string::npos)
                            return std::nullopt;
                        key.Text = expression.substr(i + 1, end - i - 1);
                        i        = end + 1;
                    }
                    else
                    {
                        const size_t start = i;
                        if (expression[i] == '-')
                            ++i;
                        while (i < expression.size() && std::isdigit(static_cast<unsigned char>(expression[i])))
                            ++i;
                        if (i == start || expression[i - 1] == '-')
                            return std::nullopt;
                        key.IsNumber = true;
                        key.Number   = std::strtoll(expression.c_str() + start, nullptr, 10);
                    }
                    if (i >= expression.size() || expression[i] != ']')
                        return std::nullopt;
                    ++i;
                }
                else
                {
                    return std::nullopt;
                }
                path.Keys.push_back(std::move(key));
            }
            return path;
        }

        // Pushes a literal a client may assign: a number, a quoted string, true or false.
        bool PushLiteral(lua_State* state, const std::string& text)
        {
            if (text == "true" || text == "false")
            {
                lua_pushboolean(state, text == "true");
                return true;
            }
            if (text.size() >= 2 && (text.front() == '"' || text.front() == '\'') && text.back() == text.front())
            {
                lua_pushlstring(state, text.data() + 1, text.size() - 2);
                return true;
            }
            return !text.empty() && lua_stringtonumber(state, text.c_str()) == text.size() + 1;
        }

        int ToStringProtected(lua_State* state)
        {
            luaL_tolstring(state, 1, nullptr);
            return 1;
        }

        int GetProtected(lua_State* state)
        {
            lua_gettable(state, 1);
            return 1;
        }

        // The value at index through tostring and its __tostring, never raising.
        std::string SafeToString(lua_State* state, int index)
        {
            index = lua_absindex(state, index);
            lua_pushcfunction(state, &ToStringProtected);
            lua_pushvalue(state, index);
            std::string text = lua_pcall(state, 1, 1, 0) == LUA_OK ? lua_tostring(state, -1) : "<__tostring failed>";
            lua_pop(state, 1);
            return text;
        }

        bool HasToString(lua_State* state, int index)
        {
            if (luaL_getmetafield(state, index, "__tostring") == LUA_TNIL)
                return false;
            lua_pop(state, 1);
            return true;
        }

        // A key as the client shows it, and how keys sort: numbers first, by value.
        struct TableField
        {
            bool         IsNumber = false;
            lua_Number   Number   = 0;
            VariableInfo Info;
        };
    }

    void VariableStore::Begin(lua_State* state)
    {
        Clear();
        m_State = state;
        lua_newtable(m_State);
        m_AnchorRef = luaL_ref(m_State, LUA_REGISTRYINDEX);
    }

    void VariableStore::Clear()
    {
        if (m_State && m_AnchorRef != LUA_NOREF)
            luaL_unref(m_State, LUA_REGISTRYINDEX, m_AnchorRef);
        m_State      = nullptr;
        m_AnchorRef  = LUA_NOREF;
        m_AnchorSize = 0;
        m_Entries.clear();
        m_TableReferences.clear();
    }

    int VariableStore::ReferenceTable()
    {
        const void* table = lua_topointer(m_State, -1);
        if (const auto it = m_TableReferences.find(table); it != m_TableReferences.end())
            return it->second;
        lua_rawgeti(m_State, LUA_REGISTRYINDEX, m_AnchorRef);
        lua_pushvalue(m_State, -2);
        lua_rawseti(m_State, -2, ++m_AnchorSize);
        lua_pop(m_State, 1);
        const int reference          = m_NextReference++;
        m_Entries[reference]         = { Kind::Table, 0, m_AnchorSize };
        m_TableReferences[table]     = reference;
        return reference;
    }

    VariableInfo VariableStore::DescribeTop(std::string name)
    {
        VariableInfo info;
        info.Name       = std::move(name);
        const int type  = lua_type(m_State, -1);
        info.Type       = lua_typename(m_State, type);
        switch (type)
        {
        case LUA_TNIL:
            info.Value = "nil";
            break;
        case LUA_TBOOLEAN:
            info.Value = lua_toboolean(m_State, -1) ? "true" : "false";
            break;
        case LUA_TNUMBER:
        case LUA_TSTRING:
        {
            lua_pushvalue(m_State, -1); // lua_tostring converts a number in place
            size_t      length = 0;
            const char* text   = lua_tolstring(m_State, -1, &length);
            std::string value(text, std::min(length, MAX_STRING_SHOWN));
            info.Value = type == LUA_TSTRING ? "\"" + value + (length > MAX_STRING_SHOWN ? "...\"" : "\"") : value;
            lua_pop(m_State, 1);
            break;
        }
        case LUA_TTABLE:
        {
            if (HasToString(m_State, -1))
            {
                info.Value = SafeToString(m_State, -1);
            }
            else
            {
                size_t entries = 0;
                lua_pushnil(m_State);
                while (lua_next(m_State, -2))
                {
                    ++entries;
                    lua_pop(m_State, 1);
                }
                info.Value = "table (" + std::to_string(entries) + (entries == 1 ? " entry)" : " entries)");
            }
            info.Reference = ReferenceTable();
            break;
        }
        case LUA_TUSERDATA:
        case LUA_TLIGHTUSERDATA:
            info.Value = HasToString(m_State, -1) ? SafeToString(m_State, -1) : "userdata";
            break;
        default:
            info.Value = info.Type;
            break;
        }
        lua_pop(m_State, 1);
        return info;
    }

    bool VariableStore::PushEnvironment(int level)
    {
        lua_Debug frame{};
        if (!lua_getstack(m_State, level, &frame))
            return false;
        int environment = 0;
        for (int i = 1; const char* name = lua_getlocal(m_State, &frame, i); ++i)
        {
            if (std::string_view(name) == "_ENV")
                environment = i;
            lua_pop(m_State, 1);
        }
        if (environment != 0)
        {
            lua_getlocal(m_State, &frame, environment);
            return true;
        }
        lua_getinfo(m_State, "f", &frame);
        for (int i = 1; const char* name = lua_getupvalue(m_State, -1, i); ++i)
        {
            if (std::string_view(name) == "_ENV")
            {
                lua_remove(m_State, -2);
                return true;
            }
            lua_pop(m_State, 1);
        }
        lua_pop(m_State, 1);
        lua_pushglobaltable(m_State);
        return true;
    }

    bool VariableStore::PushName(int level, const std::string& name)
    {
        lua_Debug frame{};
        if (!lua_getstack(m_State, level, &frame))
            return false;
        int local = 0;
        for (int i = 1; const char* localName = lua_getlocal(m_State, &frame, i); ++i)
        {
            if (name == localName)
                local = i; // the innermost of shadowed locals is the last
            lua_pop(m_State, 1);
        }
        if (local != 0)
        {
            lua_getlocal(m_State, &frame, local);
            return true;
        }
        lua_getinfo(m_State, "f", &frame);
        for (int i = 1; const char* upvalueName = lua_getupvalue(m_State, -1, i); ++i)
        {
            if (name == upvalueName)
            {
                lua_remove(m_State, -2);
                return true;
            }
            lua_pop(m_State, 1);
        }
        lua_pop(m_State, 1);
        if (!PushEnvironment(level) || !lua_istable(m_State, -1))
        {
            lua_pop(m_State, 1);
            return false;
        }
        lua_pushstring(m_State, name.c_str());
        lua_rawget(m_State, -2);
        lua_remove(m_State, -2);
        if (!lua_isnil(m_State, -1))
            return true;
        lua_pop(m_State, 1);
        return false;
    }

    std::vector<ScopeInfo> VariableStore::GetScopes(int frameId)
    {
        lua_Debug frame{};
        if (!m_State || !lua_getstack(m_State, frameId - 1, &frame))
            return {};
        const int locals   = m_NextReference++;
        const int upvalues = m_NextReference++;
        m_Entries[locals]   = { Kind::Locals, frameId - 1, 0 };
        m_Entries[upvalues] = { Kind::Upvalues, frameId - 1, 0 };
        PushEnvironment(frameId - 1);
        const int globals = ReferenceTable();
        lua_pop(m_State, 1);
        return { { "Locals", locals }, { "Upvalues", upvalues }, { "Globals", globals } };
    }

    std::optional<std::vector<VariableInfo>> VariableStore::GetVariables(int reference, int start, int count)
    {
        const auto entry = m_Entries.find(reference);
        if (!m_State || entry == m_Entries.end())
            return std::nullopt;

        std::vector<VariableInfo> variables;
        lua_Debug                 frame{};
        if (entry->second.Kind == Kind::Locals && lua_getstack(m_State, entry->second.Level, &frame))
        {
            for (int i = 1; const char* name = lua_getlocal(m_State, &frame, i); ++i)
            {
                if (name[0] == '(') // temporaries such as "(for state)"
                    lua_pop(m_State, 1);
                else
                    variables.push_back(DescribeTop(name));
            }
        }
        else if (entry->second.Kind == Kind::Upvalues && lua_getstack(m_State, entry->second.Level, &frame))
        {
            lua_getinfo(m_State, "f", &frame);
            for (int i = 1; const char* name = lua_getupvalue(m_State, -1, i); ++i)
            {
                if (std::string_view(name) == "_ENV") // shown as the Globals scope
                    lua_pop(m_State, 1);
                else
                    variables.push_back(DescribeTop(name[0] ? name : "?"));
            }
            lua_pop(m_State, 1);
        }
        else if (entry->second.Kind == Kind::Table)
        {
            lua_rawgeti(m_State, LUA_REGISTRYINDEX, m_AnchorRef);
            lua_rawgeti(m_State, -1, entry->second.Anchor);
            std::vector<TableField> fields;
            lua_pushnil(m_State);
            while (lua_next(m_State, -2))
            {
                TableField field;
                std::string name;
                const int   keyType = lua_type(m_State, -2);
                if (keyType == LUA_TSTRING)
                {
                    name = lua_tostring(m_State, -2);
                }
                else if (keyType == LUA_TNUMBER)
                {
                    field.IsNumber = true;
                    field.Number   = lua_tonumber(m_State, -2);
                    lua_pushvalue(m_State, -2);
                    name = "[" + std::string(lua_tostring(m_State, -1)) + "]";
                    lua_pop(m_State, 1);
                }
                else
                {
                    name = "[" + (keyType == LUA_TBOOLEAN ? std::string(lua_toboolean(m_State, -2) ? "true" : "false")
                                                          : SafeToString(m_State, -2)) + "]";
                }
                field.Info = DescribeTop(std::move(name));
                fields.push_back(std::move(field));
            }
            lua_pop(m_State, 2);
            std::sort(fields.begin(), fields.end(),
                      [](const TableField& a, const TableField& b)
                      {
                          if (a.IsNumber != b.IsNumber)
                              return a.IsNumber;
                          return a.IsNumber ? a.Number < b.Number : a.Info.Name < b.Info.Name;
                      });
            for (TableField& field : fields)
                variables.push_back(std::move(field.Info));
        }

        const size_t first = std::min(variables.size(), static_cast<size_t>(std::max(0, start)));
        const size_t last  = count > 0 ? std::min(variables.size(), first + static_cast<size_t>(count)) : variables.size();
        return std::vector<VariableInfo>(std::make_move_iterator(variables.begin() + first),
                                         std::make_move_iterator(variables.begin() + last));
    }

    VariableResult VariableStore::Evaluate(const std::string& expression, int frameId)
    {
        const std::optional<FieldPath> path = ParsePath(expression);
        if (!path)
            return { false, "only names and field paths (a.b[3]['c']) can be evaluated; '" + expression + "' is not one", {} };
        if (!m_State)
            return { false, "the script is not stopped", {} };
        if (!PushName(std::max(frameId, 1) - 1, path->Root))
            return { false, "no variable named '" + path->Root + "'", {} };

        std::string reached = path->Root;
        for (const PathKey& key : path->Keys)
        {
            const int type = lua_type(m_State, -1);
            if (key.IsNumber)
                lua_pushinteger(m_State, key.Number);
            else
                lua_pushstring(m_State, key.Text.c_str());

            if (type == LUA_TTABLE)
            {
                lua_rawget(m_State, -2);
            }
            else if (type == LUA_TUSERDATA && luaL_getmetafield(m_State, -2, "__index") != LUA_TNIL)
            {
                // The userdata's own field access (a Vector3's x), protected.
                lua_pop(m_State, 1);
                lua_pushcfunction(m_State, &GetProtected);
                lua_insert(m_State, -3);
                if (lua_pcall(m_State, 2, 1, 0) != LUA_OK)
                {
                    lua_pop(m_State, 1);
                    return { false, "'" + reached + "' cannot be indexed", {} };
                }
                reached += key.IsNumber ? "[" + std::to_string(key.Number) + "]" : "." + key.Text;
                continue;
            }
            else
            {
                lua_pop(m_State, 2);
                return { false, "'" + reached + "' is a " + lua_typename(m_State, type) + ", not a table", {} };
            }
            lua_remove(m_State, -2);
            reached += key.IsNumber ? "[" + std::to_string(key.Number) + "]" : "." + key.Text;
        }
        return { true, {}, DescribeTop(expression) };
    }

    VariableResult VariableStore::SetVariable(int reference, const std::string& name, const std::string& value)
    {
        const auto entry = m_Entries.find(reference);
        if (!m_State || entry == m_Entries.end())
            return { false, "the variable reference is no longer valid", {} };
        if (!PushLiteral(m_State, value))
            return { false, "only numbers, quoted strings and booleans can be set; '" + value + "' is not one", {} };
        const int literal = lua_gettop(m_State);

        lua_Debug frame{};
        bool      assigned = false;
        if (entry->second.Kind == Kind::Locals && lua_getstack(m_State, entry->second.Level, &frame))
        {
            int local = 0;
            for (int i = 1; const char* localName = lua_getlocal(m_State, &frame, i); ++i)
            {
                if (name == localName)
                    local = i;
                lua_pop(m_State, 1);
            }
            if (local != 0)
            {
                lua_pushvalue(m_State, literal);
                lua_setlocal(m_State, &frame, local);
                assigned = true;
            }
        }
        else if (entry->second.Kind == Kind::Upvalues && lua_getstack(m_State, entry->second.Level, &frame))
        {
            lua_getinfo(m_State, "f", &frame);
            for (int i = 1; const char* upvalueName = lua_getupvalue(m_State, -1, i); ++i)
            {
                lua_pop(m_State, 1);
                if (name == upvalueName)
                {
                    lua_pushvalue(m_State, literal);
                    lua_setupvalue(m_State, -2, i);
                    assigned = true;
                    break;
                }
            }
            lua_pop(m_State, 1);
        }
        else if (entry->second.Kind == Kind::Table)
        {
            lua_rawgeti(m_State, LUA_REGISTRYINDEX, m_AnchorRef);
            lua_rawgeti(m_State, -1, entry->second.Anchor);
            // A name as GetVariables shows it: "[3]" for a number key, the text otherwise.
            const bool numberKey = name.size() > 2 && name.front() == '[' && name.back() == ']' &&
                                   lua_stringtonumber(m_State, name.substr(1, name.size() - 2).c_str()) != 0;
            if (!numberKey) // lua_stringtonumber pushed the number key itself
                lua_pushstring(m_State, name.c_str());
            lua_pushvalue(m_State, literal);
            lua_rawset(m_State, -3);
            lua_pop(m_State, 2);
            assigned = true;
        }
        if (!assigned)
        {
            lua_settop(m_State, literal - 1);
            return { false, "no variable named '" + name + "' here", {} };
        }
        lua_settop(m_State, literal);
        return { true, {}, DescribeTop(name) };
    }
}
