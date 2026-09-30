#include "doctest/doctest/doctest.h"

#include "HedgehogScripting/api/ScriptVM.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include <memory>
#include <string>

using HedgehogScripting::ScriptVM;

namespace
{
    // A file system with "assets://" mounted on a fresh temp directory.
    struct Fixture
    {
        TempDir               Dir;
        FS::FileSystemManager FileSystem;

        Fixture()
        {
            auto fs = std::make_unique<FS::FileSystem>();
            fs->RegisterPath("assets://", Dir.Path());
            REQUIRE(FileSystem.Register(std::move(fs)));
        }
    };

    // Runs source and returns whether it succeeded.
    bool Runs(ScriptVM& vm, const std::string& source)
    {
        return vm.Run(source, "=test");
    }
}

TEST_CASE("ScriptVM - the allowed libraries work")
{
    Fixture  fixture;
    ScriptVM vm(fixture.FileSystem);

    REQUIRE(Runs(vm, R"(
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

TEST_CASE("ScriptVM - libraries that reach outside the VM are gone")
{
    Fixture  fixture;
    ScriptVM vm(fixture.FileSystem);

    REQUIRE(Runs(vm, R"(
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
    CHECK_FALSE(Runs(vm, "io.open('x')"));
    CHECK_FALSE(Runs(vm, "os.execute('echo hi')"));
    CHECK_FALSE(Runs(vm, "require('os')"));
    CHECK_FALSE(Runs(vm, "dofile('x.lua')"));
    CHECK_FALSE(Runs(vm, "loadfile('x.lua')"));
    CHECK_FALSE(Runs(vm, "collectgarbage('collect')"));
}

TEST_CASE("ScriptVM - binary chunks are refused")
{
    Fixture  fixture;
    ScriptVM vm(fixture.FileSystem);

    // load() inside a script returns nil plus a message for a binary chunk.
    REQUIRE(Runs(vm, R"(
        local f, message = load(string.dump(function() return 1 end))
        assert(f == nil, "a binary chunk loaded")
        assert(message:find("binary") ~= nil, message)
    )"));

    // The host refuses one too.
    const std::string binary = "\x1bLua";
    CHECK_FALSE(vm.Load(binary, "=binary").has_value());
}

TEST_CASE("ScriptVM - a runtime error names the @assets:// chunk, the line and a traceback")
{
    Fixture fixture;
    fixture.Dir.WriteFile("Scripts/Broken.lua",
        "local function inner()\n"
        "    error('boom')\n"
        "end\n"
        "inner()\n");
    ScriptVM vm(fixture.FileSystem);

    CHECK_FALSE(vm.RunFile("assets://Scripts/Broken.lua"));

    const std::string& error = vm.GetLastError();
    CHECK(error.find("assets://Scripts/Broken.lua:2:") != std::string::npos);
    CHECK(error.find("boom") != std::string::npos);
    CHECK(error.find("stack traceback") != std::string::npos);
    CHECK(error.find("assets://Scripts/Broken.lua:4:") != std::string::npos); // the caller's frame
}

TEST_CASE("ScriptVM - a syntax error names the chunk and the line")
{
    Fixture fixture;
    fixture.Dir.WriteFile("Scripts/Syntax.lua", "local x = \n\n = 3\n");
    ScriptVM vm(fixture.FileSystem);

    CHECK_FALSE(vm.LoadFile("assets://Scripts/Syntax.lua").has_value());
    CHECK(vm.GetLastError().find("assets://Scripts/Syntax.lua:3:") != std::string::npos);
}

TEST_CASE("ScriptVM - files load through the engine file system")
{
    Fixture fixture;
    fixture.Dir.WriteFile("Scripts/Answer.lua", "answer = 6 * 7\n");
    ScriptVM vm(fixture.FileSystem);

    REQUIRE(vm.RunFile("assets://Scripts/Answer.lua"));
    CHECK(vm.GetState()["answer"].get<int>() == 42);

    CHECK_FALSE(vm.RunFile("assets://Scripts/Missing.lua"));
    CHECK(vm.GetLastError().find("Missing.lua") != std::string::npos);
}

TEST_CASE("ScriptVM - a loaded function can be called again, and errors in it are caught")
{
    Fixture  fixture;
    ScriptVM vm(fixture.FileSystem);

    REQUIRE(Runs(vm, "counter = 0; function bump() counter = counter + 1 end; function fail() error('no') end"));
    const sol::protected_function bump = vm.GetState()["bump"];
    REQUIRE(vm.Call(bump));
    REQUIRE(vm.Call(bump));
    CHECK(vm.GetState()["counter"].get<int>() == 2);

    const sol::protected_function fail = vm.GetState()["fail"];
    CHECK_FALSE(vm.Call(fail));
    CHECK(vm.GetLastError().find("stack traceback") != std::string::npos);
}
