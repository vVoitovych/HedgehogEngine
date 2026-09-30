#include "doctest/doctest/doctest.h"

#include "test_log_capture.hpp"

#include "HedgehogScripting/src/Sandbox.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include <memory>
#include <string>

using HedgehogScripting::CallProtected;
using HedgehogScripting::LoadScriptFile;
using HedgehogScripting::LoadScriptSource;
using HedgehogScripting::OpenSandbox;

namespace
{
    // A sandboxed state, and a file system with "assets://" mounted on a fresh temp directory.
    struct Fixture
    {
        TempDir                 Dir;
        FS::FileSystemManager   FileSystem;
        sol::state              Lua;
        sol::protected_function Traceback;

        Fixture()
        {
            auto fs = std::make_unique<FS::FileSystem>();
            fs->RegisterPath("assets://", Dir.Path());
            REQUIRE(FileSystem.Register(std::move(fs)));
            Traceback = OpenSandbox(Lua);
        }

        // Compiles and runs source; whether both succeeded.
        bool Runs(const std::string& source)
        {
            const auto chunk = LoadScriptSource(Lua, source, "=test", Traceback);
            return chunk && CallProtected(*chunk);
        }
    };
}

TEST_CASE("Sandbox - the allowed libraries work")
{
    Fixture fixture;

    REQUIRE(fixture.Runs(R"(
        assert(math.floor(2.7) == 2)
        assert(math.sqrt(16) == 4)
        assert(string.upper("hog") == "HOG")
        assert(("a,b"):find(",") == 2)
        local t = { 3, 1, 2 }
        table.sort(t)
        assert(table.concat(t, " ") == "1 2 3")
        local co = coroutine.create(function(a) local b = coroutine.yield(a + 1); return b * 2 end)
        local _, first = coroutine.resume(co, 1)
        local _, second = coroutine.resume(co, 5)
        assert(first == 2 and second == 10)
        assert(utf8.len("h\u{E9}") == 2)
        assert(type(os.clock()) == "number")
        assert(type(os.time()) == "number")
        assert(collectgarbage("count") > 0)
        assert(load("return 1 + 2")() == 3)
    )"));
}

TEST_CASE("Sandbox - libraries that reach outside the VM are gone")
{
    Fixture fixture;

    REQUIRE(fixture.Runs(R"(
        assert(io == nil, "io")
        assert(debug == nil, "debug")
        assert(package == nil, "package")
        assert(require == nil, "require")
        assert(dofile == nil, "dofile")
        assert(loadfile == nil, "loadfile")
        assert(os.execute == nil, "os.execute")
        assert(os.remove == nil, "os.remove")
        assert(os.getenv == nil, "os.getenv")
        assert(os.exit == nil, "os.exit")
    )"));

    // Calling them fails as a script error, not a crash.
    CHECK_FALSE(fixture.Runs("io.open('x')"));
    CHECK_FALSE(fixture.Runs("os.execute('echo hi')"));
    CHECK_FALSE(fixture.Runs("os.getenv('PATH')"));
    CHECK_FALSE(fixture.Runs("require('os')"));
    CHECK_FALSE(fixture.Runs("dofile('x.lua')"));
    CHECK_FALSE(fixture.Runs("loadfile('x.lua')"));
    CHECK_FALSE(fixture.Runs("debug.traceback()"));
    CHECK_FALSE(fixture.Runs("package.loaded.x = 1"));
}

TEST_CASE("Sandbox - collectgarbage allows only \"count\"")
{
    Fixture fixture;

    CHECK(fixture.Runs("assert(type(collectgarbage('count')) == 'number')"));
    CHECK_FALSE(fixture.Runs("collectgarbage('collect')"));
    CHECK_FALSE(fixture.Runs("collectgarbage()"));
}

TEST_CASE("Sandbox - binary chunks are refused")
{
    Fixture fixture;

    // load() inside a script returns nil plus a message for a binary chunk.
    REQUIRE(fixture.Runs(R"(
        local f, message = load(string.dump(function() return 1 end))
        assert(f == nil, "a binary chunk loaded")
        assert(message:find("binary") ~= nil, message)
    )"));

    // The host refuses one too.
    const std::string binary = "\x1bLua";
    CHECK_FALSE(LoadScriptSource(fixture.Lua, binary, "=binary", fixture.Traceback).has_value());
}

TEST_CASE("Sandbox - print writes one Logger info line")
{
    Fixture fixture;

    LogCapture capture;
    REQUIRE(fixture.Runs("print('x')"));

    const auto infoLines = capture.Lines("[INFO]");
    REQUIRE(infoLines.size() == 1);
    CHECK(infoLines[0].rfind("[INFO]x", 0) == 0);
    CHECK(infoLines[0].find_first_not_of(' ', std::string("[INFO]x").size()) == std::string::npos);
}

TEST_CASE("Sandbox - print joins its arguments through tostring")
{
    Fixture fixture;

    LogCapture capture;
    REQUIRE(fixture.Runs("print('a', 2, true, nil)"));
    REQUIRE(capture.Lines("[INFO]").size() == 1);
    CHECK(capture.Lines("[INFO]a 2 true nil").size() == 1);

    // A failing __tostring is a script error, and nothing is printed.
    LogCapture failing;
    CHECK_FALSE(fixture.Runs("print(setmetatable({}, { __tostring = function() error('bad') end }))"));
    CHECK(failing.Lines("[INFO]").empty());
}

TEST_CASE("Sandbox - a runtime error names the @assets:// chunk, the line and a traceback")
{
    Fixture fixture;
    fixture.Dir.WriteFile("Scripts/Broken.lua",
                          "local function inner()\n"
                          "    error('boom')\n"
                          "end\n"
                          "inner()\n");

    const auto chunk = LoadScriptFile(fixture.Lua, fixture.FileSystem, "assets://Scripts/Broken.lua",
                                      fixture.Traceback);
    REQUIRE(chunk.has_value());

    LogCapture capture;
    CHECK_FALSE(CallProtected(*chunk));

    const std::string log = capture.Text();
    CHECK(capture.Lines("[ERROR]").size() == 1);
    CHECK(log.find("assets://Scripts/Broken.lua:2:") != std::string::npos);
    CHECK(log.find("boom") != std::string::npos);
    CHECK(log.find("stack traceback") != std::string::npos);
    CHECK(log.find("assets://Scripts/Broken.lua:4:") != std::string::npos); // the caller's frame
}

TEST_CASE("Sandbox - a syntax error names the chunk and the line, and nothing runs")
{
    Fixture fixture;
    fixture.Dir.WriteFile("Scripts/Syntax.lua", "ran = true\nlocal x = \n\n = 3\n");

    LogCapture capture;
    CHECK_FALSE(LoadScriptFile(fixture.Lua, fixture.FileSystem, "assets://Scripts/Syntax.lua", fixture.Traceback)
                    .has_value());
    CHECK(capture.Lines("[ERROR]").size() == 1);
    CHECK(capture.Text().find("assets://Scripts/Syntax.lua:4:") != std::string::npos);
    CHECK_FALSE(fixture.Lua["ran"].valid());
}

TEST_CASE("Sandbox - files load through the engine file system; a missing one is one error")
{
    Fixture fixture;
    fixture.Dir.WriteFile("Scripts/Answer.lua", "answer = 6 * 7\n");

    const auto chunk = LoadScriptFile(fixture.Lua, fixture.FileSystem, "assets://Scripts/Answer.lua",
                                      fixture.Traceback);
    REQUIRE(chunk.has_value());
    REQUIRE(CallProtected(*chunk));
    CHECK(fixture.Lua["answer"].get<int>() == 42);

    LogCapture capture;
    CHECK_FALSE(LoadScriptFile(fixture.Lua, fixture.FileSystem, "assets://Scripts/Missing.lua", fixture.Traceback)
                    .has_value());
    const auto errors = capture.Lines("[ERROR]");
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].find("Missing.lua") != std::string::npos);
}

TEST_CASE("Sandbox - a function taken from the state can be called again, with a traceback on error")
{
    Fixture fixture;

    REQUIRE(fixture.Runs("counter = 0; function bump(n) counter = counter + n end; function fail() error('no') end"));
    const sol::protected_function bump = fixture.Lua["bump"];
    REQUIRE(CallProtected(bump, 2));
    REQUIRE(CallProtected(bump, 3));
    CHECK(fixture.Lua["counter"].get<int>() == 5);

    LogCapture                    capture;
    const sol::protected_function fail = fixture.Lua["fail"];
    CHECK_FALSE(CallProtected(fail));
    CHECK(capture.Text().find("stack traceback") != std::string::npos);
}
