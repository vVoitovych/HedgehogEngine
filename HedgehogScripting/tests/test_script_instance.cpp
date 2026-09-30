#include "doctest/doctest/doctest.h"

#include "HedgehogScripting/api/ScriptClassCache.hpp"
#include "HedgehogScripting/api/ScriptInstance.hpp"
#include "HedgehogScripting/api/ScriptVM.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/api/PathUtils.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include <memory>
#include <optional>
#include <string>

using HedgehogScripting::ScriptClassCache;
using HedgehogScripting::ScriptInstance;
using HedgehogScripting::ScriptVM;

namespace
{
    // The same base class the engine ships, so scripts written for it load here.
    constexpr const char* BASE_SCRIPT = R"lua(ActorScript = {}
ActorScript.__index = ActorScript

function ActorScript:new()
    local self = setmetatable({}, ActorScript)
    return self
end

function ActorScript:OnUpdate(dt)
end
)lua";

    // Counts its own updates in a plain global: per instance, if environments work.
    constexpr const char* COUNTER_SCRIPT = R"lua(Counter = setmetatable({}, { __index = ActorScript })
Counter.__index = Counter

count = 0
step = 1

function Counter:new()
    local self = setmetatable(ActorScript:new(), Counter)
    self.created = true
    return self
end

function Counter:OnUpdate(dt)
    count = count + step
end
)lua";

    // Line 5 errors on its second update.
    constexpr const char* FAULTY_SCRIPT = R"lua(Faulty = setmetatable({}, { __index = ActorScript })
Faulty.__index = Faulty
calls = 0
function Faulty:new() return setmetatable(ActorScript:new(), Faulty) end
function Faulty:OnUpdate(dt) calls = calls + 1; if calls == 2 then error("went wrong") end end
function Faulty:Poke() error("poked") end
)lua";

    // "assets://" on a temp directory holding the base class and the given scripts.
    struct Fixture
    {
        TempDir               Dir;
        FS::FileSystemManager FileSystem;

        Fixture()
        {
            Dir.WriteFile("Scripts/Base/ActorScript.lua", BASE_SCRIPT);
            Dir.WriteFile("Scripts/Counter.lua", COUNTER_SCRIPT);
            Dir.WriteFile("Scripts/Faulty.lua", FAULTY_SCRIPT);
            auto fs = std::make_unique<FS::FileSystem>();
            fs->RegisterPath("assets://", Dir.Path());
            REQUIRE(FileSystem.Register(std::move(fs)));
        }
    };

    int CountOf(ScriptInstance& instance)
    {
        return instance.GetEnvironment()["count"].get<int>();
    }
}

TEST_CASE("ScriptClassCache - two instances share the class but not their globals")
{
    Fixture          fixture;
    ScriptVM         vm(fixture.FileSystem);
    ScriptClassCache cache(vm);
    REQUIRE(cache.IsBaseLoaded());

    std::optional<ScriptInstance> a = cache.CreateInstance("assets://Scripts/Counter.lua", 1, "A");
    std::optional<ScriptInstance> b = cache.CreateInstance("assets://Scripts/Counter.lua", 2, "B");
    REQUIRE(a.has_value());
    REQUIRE(b.has_value());

    // One class table, two selves.
    CHECK(a->GetEnvironment()["Counter"].get<sol::table>() == b->GetEnvironment()["Counter"].get<sol::table>());
    CHECK(a->GetSelf() != b->GetSelf());
    CHECK(a->GetSelf()["created"].get<bool>());

    b->GetEnvironment()["step"] = 10;
    for (int i = 0; i < 3; ++i)
        REQUIRE(a->Call("OnUpdate", 0.016f));
    REQUIRE(b->Call("OnUpdate", 0.016f));

    CHECK(CountOf(*a) == 3);
    CHECK(CountOf(*b) == 10);
    CHECK(a->GetEntity() == 1u);
    CHECK(b->GetEntityName() == "B");
}

TEST_CASE("ScriptClassCache - a script compiles once however many instances it has")
{
    Fixture          fixture;
    ScriptVM         vm(fixture.FileSystem);
    ScriptClassCache cache(vm);
    const int afterBase = cache.GetCompileCount();
    CHECK(afterBase == 1);

    for (ECS::Entity entity = 0; entity < 5; ++entity)
        REQUIRE(cache.CreateInstance("assets://Scripts/Counter.lua", entity, "E").has_value());
    CHECK(cache.GetCompileCount() == afterBase + 1);

    // Everything lives in the one VM.
    CHECK(ScriptVM::GetOpenStateCount() == 1);
}

TEST_CASE("ScriptInstance - an error in OnUpdate names the entity, file and line, then faults it")
{
    Fixture          fixture;
    ScriptVM         vm(fixture.FileSystem);
    ScriptClassCache cache(vm);
    std::optional<ScriptInstance> faulty = cache.CreateInstance("assets://Scripts/Faulty.lua", 7, "Turret");
    REQUIRE(faulty.has_value());

    CHECK(faulty->Call("OnUpdate", 0.1f));
    CHECK_FALSE(faulty->IsFaulted());

    CHECK_FALSE(faulty->Call("OnUpdate", 0.1f));
    CHECK(faulty->IsFaulted());

    const std::string& error = faulty->GetLastError();
    CHECK(error.find("[Script] Turret (assets://Scripts/Faulty.lua): ") == 0);
    CHECK(error.find("assets://Scripts/Faulty.lua:5:") != std::string::npos);
    CHECK(error.find("went wrong") != std::string::npos);
    CHECK(error.find("stack traceback") != std::string::npos);

    // Faulted: skipped from now on, so the error is not logged again every frame.
    CHECK_FALSE(faulty->Call("OnUpdate", 0.1f));
    CHECK(faulty->GetEnvironment()["calls"].get<int>() == 2);
}

TEST_CASE("ScriptInstance - an error outside OnStart and OnUpdate reports but does not fault")
{
    Fixture          fixture;
    ScriptVM         vm(fixture.FileSystem);
    ScriptClassCache cache(vm);
    std::optional<ScriptInstance> faulty = cache.CreateInstance("assets://Scripts/Faulty.lua", 7, "Turret");
    REQUIRE(faulty.has_value());

    CHECK_FALSE(faulty->Call("Poke"));
    CHECK_FALSE(faulty->IsFaulted());
    CHECK(faulty->GetLastError().find("poked") != std::string::npos);
}

TEST_CASE("ScriptInstance - a method the class lacks is not an error")
{
    Fixture          fixture;
    ScriptVM         vm(fixture.FileSystem);
    ScriptClassCache cache(vm);
    std::optional<ScriptInstance> counter = cache.CreateInstance("assets://Scripts/Counter.lua", 1, "A");
    REQUIRE(counter.has_value());

    CHECK_FALSE(counter->HasMethod("OnStart"));
    CHECK(counter->HasMethod("OnUpdate")); // Counter's own
    CHECK(counter->HasMethod("new"));      // inherited from ActorScript
    CHECK(counter->Call("OnStart"));
}

TEST_CASE("ScriptClassCache - a missing file or class makes no instance")
{
    Fixture fixture;
    fixture.Dir.WriteFile("Scripts/Nameless.lua", "Something = {}\n");
    ScriptVM         vm(fixture.FileSystem);
    ScriptClassCache cache(vm);

    CHECK_FALSE(cache.CreateInstance("assets://Scripts/Missing.lua", 1, "A").has_value());
    CHECK_FALSE(cache.CreateInstance("assets://Scripts/Nameless.lua", 1, "A").has_value());
}

TEST_CASE("ScriptClassCache - the shipped ActorScript and PlayerScript load unchanged")
{
    // Mount the real Assets folder, read-only use.
    FS::FileSystemManager fileSystem;
    auto fs = std::make_unique<FS::FileSystem>();
    REQUIRE(fs->RegisterPath("assets://", FS::GetEngineRootDirectory() / "Assets"));
    REQUIRE(fileSystem.Register(std::move(fs)));

    ScriptVM vm(fileSystem);
    // PlayerScript calls the legacy transform bindings; stand-ins record the rotation.
    REQUIRE(vm.Run("rotationZ = 0\n"
                   "function GetRotation() return { x = 0, y = 0, z = rotationZ } end\n"
                   "function SetRotation(r) rotationZ = r.z end\n",
                   "=bindings"));

    ScriptClassCache cache(vm);
    REQUIRE(cache.IsBaseLoaded());
    std::optional<ScriptInstance> player = cache.CreateInstance("assets://Scripts/PlayerScript.lua", 3, "Player");
    REQUIRE(player.has_value());

    CHECK(player->GetEnvironment()["speed"].get<double>() == doctest::Approx(1.0));
    REQUIRE(player->Call("OnUpdate", 0.5f));
    CHECK(vm.GetState()["rotationZ"].get<double>() == doctest::Approx(0.5));
}
