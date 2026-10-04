#pragma once

#include "HedgehogLuaDebug/api/LuaDebugEngine.hpp"

#include <map>
#include <optional>
#include <string>
#include <vector>

struct lua_State;

namespace LuaDebug
{
    // The variables of one stop. Begin anchors a table in the registry that keeps every table it
    // hands a reference for alive; Clear drops it, and references keep counting up across stops,
    // so one from an earlier stop is never found again.
    class VariableStore
    {
    public:
        void Begin(lua_State* state);
        void Clear();

        [[nodiscard]] std::vector<ScopeInfo> GetScopes(int frameId);
        [[nodiscard]] std::optional<std::vector<VariableInfo>> GetVariables(int reference, int start, int count);
        [[nodiscard]] VariableResult Evaluate(const std::string& expression, int frameId);
        [[nodiscard]] VariableResult SetVariable(int reference, const std::string& name, const std::string& value);

    private:
        enum class Kind
        {
            Locals,
            Upvalues,
            Table, // a table kept in the anchor, Globals included
        };

        struct Entry
        {
            Kind Kind   = Kind::Table;
            int  Level  = 0; // Locals and Upvalues: the frame's level
            int  Anchor = 0; // Table: its slot in the anchor
        };

        // The value on the top of the stack as the client shows it (popped).
        [[nodiscard]] VariableInfo DescribeTop(std::string name);
        // A reference for the table on the top of the stack (left there), reusing one per table.
        [[nodiscard]] int ReferenceTable();
        // Pushes the frame's _ENV (a local or upvalue named _ENV, else the globals); false when the
        // level has no frame.
        bool PushEnvironment(int level);
        // Pushes the value a name has in a frame: local, then upvalue, then _ENV field.
        bool PushName(int level, const std::string& name);

    private:
        lua_State*                m_State     = nullptr;
        int                       m_AnchorRef = -2; // LUA_NOREF
        int                       m_AnchorSize = 0;
        int                       m_NextReference = 1;
        std::map<int, Entry>      m_Entries;
        std::map<const void*, int> m_TableReferences;
    };
}
