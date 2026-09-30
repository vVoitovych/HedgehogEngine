#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"

#include "HedgehogMath/api/Matrix.hpp"
#include "HedgehogMath/api/Quaternion.hpp"

#include "ECS/api/components/Hierarchy.hpp"

#include <cmath>
#include <cstdio>
#include <string>

using HedgehogEngine::TransformComponent;

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    // A script class named `name` with the given method bodies (empty ones are left out).
    std::string Script(const std::string& name, const std::string& onStart, const std::string& onUpdate = "")
    {
        std::string source = name + " = setmetatable({}, { __index = ActorScript })\n" + name + ".__index = " + name +
                             "\n" + "function " + name + ":new() return setmetatable(ActorScript:new(), " + name +
                             ") end\n";
        if (!onStart.empty())
            source += "function " + name + ":OnStart()\n" + onStart + "\nend\n";
        if (!onUpdate.empty())
            source += "function " + name + ":OnUpdate(dt)\n" + onUpdate + "\nend\n";
        return source;
    }

    HM::Vector3 Translation(const HM::Matrix4x4& matrix)
    {
        return HM::Vector3(matrix[3].x(), matrix[3].y(), matrix[3].z());
    }

    const std::string& NameOf(EngineWorld& world, ECS::Entity entity)
    {
        return world.Ecs().GetComponent<ECS::HierarchyComponent>(entity).Name;
    }
}

TEST_CASE("Entity bindings - a position written in OnUpdate reaches ObjMatrix the same frame")
{
    EngineWorld world;
    world.WriteScript("Mover.lua", Script("Mover", "", "    self.entity.transform.position = Vector3(1, 2, 3)"));
    const ECS::Entity mover = world.AddScripted("Scripts/Mover.lua");

    REQUIRE(world.Context.Play());
    world.Frame(STEP);

    const auto& transform = world.Ecs().GetComponent<TransformComponent>(mover);
    const HM::Vector3 translation = Translation(transform.ObjMatrix);
    CHECK(translation.x() == doctest::Approx(1.0f));
    CHECK(translation.y() == doctest::Approx(2.0f));
    CHECK(translation.z() == doctest::Approx(3.0f));
    REQUIRE(world.Stop());
}

TEST_CASE("Entity bindings - under a moved parent, ObjMatrix is parent times local")
{
    EngineWorld world;
    world.WriteScript("Mover.lua", Script("Mover", "", "    self.entity.transform.position = Vector3(1, 2, 3)"));

    const ECS::Entity parent = world.Context.GetSceneManager().CreateGameObject();
    auto& parentTransform    = world.Ecs().GetComponent<TransformComponent>(parent);
    parentTransform.Position = HM::Vector3(10.0f, 0.0f, 0.0f);
    parentTransform.Rotation = HM::Vector3(0.0f, 90.0f, 0.0f);
    world.Context.GetEventBus().Publish(HedgehogEngine::TransformChangedEvent{ parent });

    const ECS::Entity child = world.Context.GetSceneManager().CreateGameObject(parent);
    HedgehogEngine::ScriptComponent script;
    script.ScriptPath = "Scripts/Mover.lua";
    world.Ecs().AddComponent(child, script);

    REQUIRE(world.Context.Play());
    world.Frame(STEP);

    const auto& parentWorld = world.Ecs().GetComponent<TransformComponent>(parent).ObjMatrix;
    const auto& childLocal  = world.Ecs().GetComponent<TransformComponent>(child).LocalMatrix;
    const auto& childWorld  = world.Ecs().GetComponent<TransformComponent>(child).ObjMatrix;
    const HM::Vector3 expected = Translation(parentWorld * childLocal);
    const HM::Vector3 actual   = Translation(childWorld);
    CHECK(actual.x() == doctest::Approx(expected.x()));
    CHECK(actual.y() == doctest::Approx(expected.y()));
    CHECK(actual.z() == doctest::Approx(expected.z()));
    CHECK(Translation(childLocal).x() == doctest::Approx(1.0f));
    CHECK(actual.x() != doctest::Approx(1.0f)); // the parent really moved it
    REQUIRE(world.Stop());
}

TEST_CASE("Entity bindings - rotation is a Quat stored as Euler degrees")
{
    EngineWorld world;
    world.WriteScript("Turner.lua", Script("Turner", R"lua(
    local t = self.entity.transform
    t.rotation = Quat.fromEuler(10, 20, 30)
    local e = t.eulerAngles
    assert(math.abs(e.x - 10) < 1e-3 and math.abs(e.y - 20) < 1e-3 and math.abs(e.z - 30) < 1e-3, tostring(e))
    t.scale = Vector3(2, 2, 2)
    t:translate(Vector3(0, 0, 5))
    print("turned")
)lua"));
    const ECS::Entity turner = world.AddScripted("Scripts/Turner.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    CHECK(log.Lines("turned").size() == 1);
    CHECK(log.Lines("[ERROR]").empty());

    const auto& transform = world.Ecs().GetComponent<TransformComponent>(turner);
    CHECK(transform.Rotation.x() == doctest::Approx(10.0f).epsilon(1e-3));
    CHECK(transform.Rotation.y() == doctest::Approx(20.0f).epsilon(1e-3));
    CHECK(transform.Rotation.z() == doctest::Approx(30.0f).epsilon(1e-3));
    CHECK(transform.Scale.x() == 2.0f);
    CHECK(transform.Position.z() == 5.0f);
    REQUIRE(world.Stop());
}

TEST_CASE("Entity bindings - forward, right and up come from the world matrix")
{
    EngineWorld world;
    world.WriteScript("Axes.lua", Script("Axes", "", R"lua(
    local function z(v) if math.abs(v) < 5e-4 then return 0 end return v end -- no "-0.000"
    local t = self.entity.transform
    local f, r, u = t.forward, t.right, t.up
    print(string.format("axes %.3f %.3f %.3f | %.3f %.3f %.3f | %.3f %.3f %.3f",
        z(f.x), z(f.y), z(f.z), z(r.x), z(r.y), z(r.z), z(u.x), z(u.y), z(u.z)))
)lua"));
    const ECS::Entity entity = world.AddScripted("Scripts/Axes.lua");
    world.Ecs().GetComponent<TransformComponent>(entity).Rotation = HM::Vector3(0.0f, 90.0f, 0.0f);
    world.Context.GetEventBus().Publish(HedgehogEngine::TransformChangedEvent{ entity });
    world.Context.GetTransformSystem()->Update(world.Ecs(), world.Context.GetEventBus());
    world.Context.GetHierarchySystem()->Update(world.Ecs(), world.Context.GetEventBus());

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);

    // Turned 90 degrees about Y: forward (-Z) goes to -X, right (+X) to -Z, and up stays +Y.
    const HM::Vector3 forward = HM::Quaternion::FromEuler(0.0f, 90.0f, 0.0f) * HM::Vector3(0.0f, 0.0f, -1.0f);
    const HM::Vector3 right   = HM::Quaternion::FromEuler(0.0f, 90.0f, 0.0f) * HM::Vector3(1.0f, 0.0f, 0.0f);
    const auto z = [](float v) { return std::abs(v) < 5e-4f ? 0.0f : v; };
    char       expected[160];
    std::snprintf(expected, sizeof(expected), "axes %.3f %.3f %.3f | %.3f %.3f %.3f | %.3f %.3f %.3f", z(forward.x()),
                  z(forward.y()), z(forward.z()), z(right.x()), z(right.y()), z(right.z()), 0.0f, 1.0f, 0.0f);
    const auto lines = log.Lines("axes ");
    REQUIRE(lines.size() == 1);
    CHECK_MESSAGE(lines[0].find(expected) != std::string::npos, lines[0] << " expected " << expected);
    REQUIRE(world.Stop());
}

TEST_CASE("Entity bindings - name reads and writes the hierarchy; parent and children match it")
{
    EngineWorld world;
    world.WriteScript("Family.lua", Script("Family", R"lua(
    local me = self.entity
    print("name " .. me.name)
    local kids = me.children
    print("children " .. #kids .. " first " .. kids[1].name)
    print("parent of first is me " .. tostring(kids[1].parent == me))
    print("my parent is nil " .. tostring(me.parent == nil))
    me.name = "Renamed"
)lua"));
    const ECS::Entity family = world.AddScripted("Scripts/Family.lua");
    const ECS::Entity kid    = world.Context.GetSceneManager().CreateGameObject(family);
    const std::string before = NameOf(world, family);

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);

    CHECK(log.Lines("name " + before).size() == 1);
    CHECK(log.Lines("children 1 first " + NameOf(world, kid)).size() == 1);
    CHECK(log.Lines("parent of first is me true").size() == 1);
    CHECK(log.Lines("my parent is nil true").size() == 1);
    CHECK(NameOf(world, family) == "Renamed");
    REQUIRE(world.Stop());
}

TEST_CASE("Entity bindings - a handle to a destroyed or recycled entity is invalid, and using it is an error")
{
    EngineWorld world;
    world.WriteScript("Keeper.lua", Script("Keeper", R"lua(
    kid = self.entity.children[1]
    print("kid valid " .. tostring(kid:isValid()))
)lua", R"lua(
    if not kid:isValid() then
        print("kid gone")
        for _, other in ipairs(self.entity.children) do
            if other.id == kid.id then
                print("same id, equal " .. tostring(other == kid))
            end
        end
        local p = kid.transform.position
    end
)lua"));
    const ECS::Entity keeper = world.AddScripted("Scripts/Keeper.lua");
    const ECS::Entity kid    = world.Context.GetSceneManager().CreateGameObject(keeper);

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    CHECK(log.Lines("kid valid true").size() == 1);

    // Delete the child, then make children until one reuses its id.
    world.Context.GetSceneManager().DeleteGameObject(kid);
    bool reused = false;
    for (int i = 0; i < 600 && !reused; ++i)
        reused = world.Context.GetSceneManager().CreateGameObject(keeper) == kid;
    REQUIRE(reused);

    world.Frame(STEP);
    CHECK(log.Lines("kid gone").size() == 1);
    CHECK(log.Lines("same id, equal false").size() == 1);
    const auto errors = log.Lines("[ERROR][Script]");
    REQUIRE(errors.size() == 1);
    CHECK(log.Text().find("Entity " + std::to_string(kid) + " (generation") != std::string::npos);
    CHECK(log.Text().find("no longer exists") != std::string::npos);
    CHECK(log.Text().find("stack traceback") != std::string::npos);
    REQUIRE(world.Stop());
}

TEST_CASE("Entity bindings - two handles to the same entity compare equal")
{
    EngineWorld world;
    world.WriteScript("Twins.lua", Script("Twins", R"lua(
    local child = self.entity.children[1]
    print("equal " .. tostring(child.parent == self.entity) .. " " .. tostring(self.entity == self.entity))
    print("text " .. tostring(self.entity))
)lua"));
    const ECS::Entity twins = world.AddScripted("Scripts/Twins.lua");
    (void)world.Context.GetSceneManager().CreateGameObject(twins);

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    CHECK(log.Lines("equal true true").size() == 1);
    CHECK(log.Lines("text Entity " + std::to_string(twins) + " (generation").size() == 1);
    REQUIRE(world.Stop());
}
