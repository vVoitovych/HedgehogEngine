#include "doctest/doctest/doctest.h"

#include "test_debug_session.hpp"

#include <optional>

using LuaDebugTest::DebugSession;
using nlohmann::json;

namespace
{
    // Lines matter.
    constexpr const char* VARIABLES_SCRIPT = "local t = { n = 1, inner = { deep = 'x' }, [1] = 10, [2] = 20 }\n" // 1
                                             "local s = 'hi'\n"                                              // 2
                                             "local function f()\n"                                          // 3
                                             "    local u = t\n"                                             // 4
                                             "    return u\n"                                                // 5
                                             "end\n"                                                         // 6
                                             "f()\n"                                                         // 7
                                             "result = t.n\n";                                               // 8

    const LuaDebug::VariableInfo* Find(const std::vector<LuaDebug::VariableInfo>& variables, const std::string& name)
    {
        for (const LuaDebug::VariableInfo& variable : variables)
            if (variable.Name == name)
                return &variable;
        return nullptr;
    }
}

TEST_CASE("Lua variables - scopes, lazily expanded tables, and references gone after continue")
{
    DebugSession session(VARIABLES_SCRIPT, "Vars.lua");
    (void)session.SetBreakpoints({ 5 });

    int tableReference = 0;
    // The client reacts to the stop: it inspects through the engine, then continues.
    session.Client->SetClient(
        [&](const std::string& body)
        {
            if (json::parse(body).value("event", "") != "stopped")
                return;
            const std::vector<LuaDebug::ScopeInfo> scopes = session.Engine.GetScopes(1);
            REQUIRE(scopes.size() == 3);
            CHECK(scopes[0].Name == "Locals");
            CHECK(scopes[1].Name == "Upvalues");
            CHECK(scopes[2].Name == "Globals");

            const auto locals = session.Engine.GetVariables(scopes[0].Reference);
            REQUIRE(locals);
            REQUIRE(locals->size() == 1);
            CHECK((*locals)[0].Name == "u");
            CHECK((*locals)[0].Value == "table (4 entries)");
            tableReference = (*locals)[0].Reference;
            CHECK(tableReference != 0);

            // The upvalue t is the same table: the same reference.
            const auto upvalues = session.Engine.GetVariables(scopes[1].Reference);
            REQUIRE(upvalues);
            REQUIRE(Find(*upvalues, "t"));
            CHECK(Find(*upvalues, "t")->Reference == tableReference);

            // Expanded: number keys first, in order, then names; nested tables expand in turn.
            const auto fields = session.Engine.GetVariables(tableReference);
            REQUIRE(fields);
            REQUIRE(fields->size() == 4);
            CHECK((*fields)[0].Name == "[1]");
            CHECK((*fields)[0].Value == "10");
            CHECK((*fields)[1].Name == "[2]");
            CHECK((*fields)[2].Name == "inner");
            CHECK((*fields)[3].Name == "n");
            CHECK((*fields)[3].Type == "number");
            const auto inner = session.Engine.GetVariables((*fields)[2].Reference);
            REQUIRE(inner);
            REQUIRE(inner->size() == 1);
            CHECK((*inner)[0].Value == "\"x\"");

            // Paging.
            const auto page = session.Engine.GetVariables(tableReference, 1, 2);
            REQUIRE(page);
            REQUIRE(page->size() == 2);
            CHECK((*page)[0].Name == "[2]");

            // A number set through the table is what the script reads after continuing.
            const LuaDebug::VariableResult set = session.Engine.SetVariable(tableReference, "n", "5");
            CHECK(set.Ok);
            CHECK(set.Value.Value == "5");
            CHECK_FALSE(session.Engine.SetVariable(tableReference, "n", "print('no')").Ok);

            session.Queue("continue");
        });
    session.Run();
    CHECK(session.Global("result") == 5);

    // The stop is over: its references are rejected, through the engine and the protocol.
    CHECK_FALSE(session.Engine.GetVariables(tableReference));
    session.Client->SetClient({});
    (void)session.Sent();
    session.Queue("variables", { { "variablesReference", tableReference } });
    session.Server.Pump();
    const std::vector<json> sent = session.Sent();
    REQUIRE(sent.size() == 1);
    CHECK(sent[0]["success"] == false);
    CHECK(sent[0]["message"] == "the variable reference is no longer valid");
}

TEST_CASE("Lua variables - evaluate resolves names and field paths and never runs code")
{
    DebugSession session(VARIABLES_SCRIPT, "Vars.lua");
    (void)session.SetBreakpoints({ 5 });

    std::vector<json> evaluated;
    session.Client->SetClient(
        [&](const std::string& body)
        {
            const json message = json::parse(body);
            if (message.value("event", "") == "stopped")
            {
                // s is a local of the main chunk, frame 2; f, frame 1, does not capture it.
                const std::pair<const char*, int> expressions[] = { { "u.inner.deep", 1 }, { "t[1]", 1 }, { "t['n']", 1 },
                                                                    { "s", 2 },            { "u.missing", 1 }, { "f()", 1 },
                                                                    { "os.exit()", 1 },    { "t.n + 1", 1 },   { "s.x", 2 },
                                                                    { "s", 1 } };
                for (const auto& [expression, frame] : expressions)
                    session.Queue("evaluate", { { "expression", expression }, { "frameId", frame } });
                session.Queue("continue");
            }
            else if (message.value("command", "") == "evaluate")
            {
                evaluated.push_back(message);
            }
        });
    session.Run();
    CHECK(session.Global("result") == 1);

    REQUIRE(evaluated.size() == 10);
    CHECK(evaluated[0]["body"]["result"] == "\"x\"");
    CHECK(evaluated[1]["body"]["result"] == "10");
    CHECK(evaluated[2]["body"]["result"] == "1");
    CHECK(evaluated[3]["body"]["result"] == "\"hi\"");
    CHECK(evaluated[4]["body"]["result"] == "nil");
    for (size_t i = 5; i < 8; ++i)
    {
        CAPTURE(i);
        CHECK(evaluated[i]["success"] == false);
        CHECK(evaluated[i]["message"].get<std::string>().starts_with("only names and field paths"));
    }
    CHECK(evaluated[8]["message"] == "'s' is a string, not a table");
    CHECK(evaluated[9]["message"] == "no variable named 's'");
}
