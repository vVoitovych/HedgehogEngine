#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"

#include "HedgehogMath/api/Quaternion.hpp"
#include "HedgehogMath/api/Vector.hpp"

#include "ECS/api/components/Hierarchy.hpp"

#include <string>

using HedgehogEngine::ScriptPropertyType;
using HedgehogEngine::ScriptComponent;

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    // A script class named `name` whose OnStart runs body. Anything it reports through print or
    // an error shows in the captured log.
    std::string ScriptWithOnStart(const std::string& name, const std::string& body)
    {
        return name + " = setmetatable({}, { __index = ActorScript })\n" + name + ".__index = " + name + "\n" +
               "function " + name + ":new() return setmetatable(ActorScript:new(), " + name + ") end\n" +
               "function " + name + ":OnStart()\n" + body + "\nend\n";
    }

    void SetNumber(ScriptComponent& component, const std::string& name, float value)
    {
        HedgehogEngine::SetScriptProperty(component, { name, ScriptPropertyType::Number, value, {} });
    }

    // Plays one frame of world with the scripts it holds, then stops.
    void PlayOneFrame(EngineWorld& world)
    {
        REQUIRE(world.Context.Play());
        world.Context.UpdatePlayMode(STEP);
        REQUIRE(world.Stop());
    }
}

TEST_CASE("Math bindings - Lua Vector3 and Quat results equal HedgehogMath's within 1e-5")
{
    // The C++ answers, handed to the script as declared properties; the script asserts against them.
    const HM::Vector3    sum   = HM::Vector3(1.0f, 2.0f, 3.0f) + HM::Vector3(1.0f, 1.0f, 1.0f);
    const HM::Vector3    cross = HM::Cross(HM::Vector3(1.0f, 0.0f, 0.0f), HM::Vector3(0.0f, 1.0f, 0.0f));
    const HM::Vector3    turn  = HM::Quaternion::FromEuler(0.0f, 90.0f, 0.0f) * HM::Vector3(1.0f, 0.0f, 0.0f);
    const HM::Quaternion a     = HM::Quaternion::FromEuler(0.0f, 0.0f, 0.0f);
    const HM::Quaternion b     = HM::Quaternion::FromEuler(30.0f, 90.0f, -45.0f);
    const HM::Quaternion half  = HM::Quaternion::Slerp(a, b, 0.5f);

    EngineWorld world;
    const std::string declared = "Properties = { sumLength = 0, crossX = 0, crossY = 0, crossZ = 0, turnX = 0, turnY = 0, "
                                 "turnZ = 0, halfX = 0, halfY = 0, halfZ = 0, halfW = 0 }\n";
    world.WriteScript("MathCheck.lua", declared + ScriptWithOnStart("MathCheck", R"lua(
    local function near(x, y) return math.abs(x - y) <= 1e-5 end

    local length = (Vector3(1, 2, 3) + Vector3(1, 1, 1)):length()
    assert(near(length, self.sumLength), "length " .. length)

    local c = Vector3(1, 0, 0):cross(Vector3(0, 1, 0))
    assert(near(c.x, self.crossX) and near(c.y, self.crossY) and near(c.z, self.crossZ), "cross " .. tostring(c))

    local r = Quat.fromEuler(0, 90, 0) * Vector3(1, 0, 0)
    assert(near(r.x, self.turnX) and near(r.y, self.turnY) and near(r.z, self.turnZ), "turn " .. tostring(r))

    local s = Quat.slerp(Quat.fromEuler(0, 0, 0), Quat.fromEuler(30, 90, -45), 0.5)
    assert(near(s.x, self.halfX) and near(s.y, self.halfY) and near(s.z, self.halfZ) and near(s.w, self.halfW), "slerp " .. tostring(s))

    print("math checked")
)lua"));

    const ECS::Entity entity    = world.AddScripted("Scripts/MathCheck.lua");
    auto&             component = world.Ecs().GetComponent<ScriptComponent>(entity);
    SetNumber(component, "sumLength", sum.LengthSlow());
    SetNumber(component, "crossX", cross.x());
    SetNumber(component, "crossY", cross.y());
    SetNumber(component, "crossZ", cross.z());
    SetNumber(component, "turnX", turn.x());
    SetNumber(component, "turnY", turn.y());
    SetNumber(component, "turnZ", turn.z());
    SetNumber(component, "halfX", half.x());
    SetNumber(component, "halfY", half.y());
    SetNumber(component, "halfZ", half.z());
    SetNumber(component, "halfW", half.w());

    LogCapture log;
    PlayOneFrame(world);
    CHECK(log.Lines("[ERROR][Script]").empty());
    CHECK(log.Lines("math checked").size() == 1);
}

TEST_CASE("Math bindings - Vector3 and Quat behave as values in Lua")
{
    EngineWorld world;
    world.WriteScript("ValueCheck.lua", ScriptWithOnStart("ValueCheck", R"lua(
    local v = Vector3(1, 2, 3)
    v.x = 4
    assert(v.x == 4 and v.y == 2 and v.z == 3, "field write")
    assert(Vector3() == Vector3(0, 0, 0), "default")
    assert(Vector3(1, 2, 3) - Vector3(1, 1, 1) == Vector3(0, 1, 2), "subtraction")
    assert(Vector3(1, 2, 3) * 2 == Vector3(2, 4, 6) and 2 * Vector3(1, 2, 3) == Vector3(2, 4, 6), "scale")
    assert(Vector3(2, 4, 6) / 2 == Vector3(1, 2, 3), "divide")
    assert(-Vector3(1, -2, 3) == Vector3(-1, 2, -3), "negate")
    assert(Vector3(1, 2, 3):dot(Vector3(4, 5, 6)) == 32, "dot")
    assert(Vector3(0, 0, 0):normalized() == Vector3(0, 0, 0), "zero stays zero")
    assert(math.abs(Vector3(3, 0, 4):normalized():length() - 1) < 1e-6, "unit length")
    assert(Vector3(0, 0, 0):lerp(Vector3(2, 4, 6), 0.5) == Vector3(1, 2, 3), "lerp")
    assert(tostring(Vector3(1, 2, 3)) == "Vector3(1, 2, 3)", tostring(Vector3(1, 2, 3)))

    local q = Quat.fromEuler(10, 20, 30)
    local back = q * q:inverse()
    assert(math.abs(back.w - 1) < 1e-5, "inverse")
    local e = q:euler()
    assert(math.abs(e.x - 10) < 1e-3 and math.abs(e.y - 20) < 1e-3 and math.abs(e.z - 30) < 1e-3, "euler")
    assert(Quat.identity() == Quat(), "identity")
    local up = Quat.axisAngle(Vector3(0, 0, 1), 90) * Vector3(1, 0, 0)
    assert(math.abs(up.y - 1) < 1e-5, "axis angle")
    local look = Quat.lookRotation(Vector3(1, 0, 0)) * Vector3(0, 0, -1)
    assert(math.abs(look.x - 1) < 1e-5, "look rotation")
    print("values checked")
)lua"));
    (void)world.AddScripted("Scripts/ValueCheck.lua");

    LogCapture log;
    PlayOneFrame(world);
    CHECK(log.Lines("[ERROR][Script]").empty());
    CHECK(log.Lines("values checked").size() == 1);
}

TEST_CASE("Math bindings - a wrong argument is a script error naming the entity and file, not a crash")
{
    EngineWorld world;
    world.WriteScript("BadMath.lua", ScriptWithOnStart("BadMath", "    local v = Vector3(1, 2, 3) + 5"));
    world.WriteScript("Survivor.lua", ScriptWithOnStart("Survivor", "    print('survivor started')"));
    const ECS::Entity bad = world.AddScripted("Scripts/BadMath.lua");
    (void)world.AddScripted("Scripts/Survivor.lua");
    const std::string badName = world.Ecs().GetComponent<ECS::HierarchyComponent>(bad).Name;

    LogCapture log;
    PlayOneFrame(world);

    const auto errors = log.Lines("[ERROR][Script]");
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].find("[Script] " + badName + " (assets://Scripts/BadMath.lua)") != std::string::npos);
    CHECK(log.Text().find("assets://Scripts/BadMath.lua:5:") != std::string::npos);
    CHECK(log.Lines("survivor started").size() == 1);
}
