#include "doctest/doctest/doctest.h"

#include "HedgehogScripting/api/ScriptVM.hpp"

#include "HedgehogMath/api/Quaternion.hpp"
#include "HedgehogMath/api/Vector.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include "test_log_capture.hpp"

#include <fstream>
#include <memory>
#include <sstream>
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

        void Write(const std::string& relativePath, const std::string& text)
        {
            const auto path = Dir.Path() / relativePath;
            std::filesystem::create_directories(path.parent_path());
            std::ofstream(path) << text;
        }
    };

    float Number(ScriptVM& vm, const char* global)
    {
        return vm.GetState()[global].get<float>();
    }
}

TEST_CASE("Bindings - Vector3 arithmetic gives the same values as HedgehogMath")
{
    Fixture  fixture;
    ScriptVM vm(fixture.FileSystem);

    REQUIRE(vm.Run(R"(
        sumLength = (Vector3(1, 2, 3) + Vector3(1, 1, 1)):length()
        local a, b = Vector3(1, 2, 3), Vector3(-2, 0.5, 4)
        dot = a:dot(b)
        local c = a:cross(b)
        cx, cy, cz = c.x, c.y, c.z
        local d = (a - b) * 2 / 4
        dx, dy, dz = d.x, d.y, d.z
        local s = 3 * a
        sx = s.x
        local n = b:normalized()
        nLength = n:length()
        local l = a:lerp(b, 0.25)
        lx, ly, lz = l.x, l.y, l.z
        local zero = Vector3():normalized()
        zeroLength = zero:length()
        local m = -a
        m.y = 10
        mx, my = m.x, m.y
        assert(Vector3(1, 2, 3) == a)
        assert(Vector3(1, 2, 3) ~= b)
        text = tostring(Vector3(1, 2.5, -3))
    )", "=vector test"));

    const HM::Vector3 a(1.0f, 2.0f, 3.0f);
    const HM::Vector3 b(-2.0f, 0.5f, 4.0f);
    CHECK(Number(vm, "sumLength") == doctest::Approx((a + HM::Vector3(1.0f, 1.0f, 1.0f)).LengthSlow()));
    CHECK(Number(vm, "dot") == doctest::Approx(HM::Dot(a, b)));
    const HM::Vector3 cross = HM::Cross(a, b);
    CHECK(Number(vm, "cx") == doctest::Approx(cross.x()));
    CHECK(Number(vm, "cy") == doctest::Approx(cross.y()));
    CHECK(Number(vm, "cz") == doctest::Approx(cross.z()));
    const HM::Vector3 d = (a - b) * 2.0f / 4.0f;
    CHECK(Number(vm, "dx") == doctest::Approx(d.x()));
    CHECK(Number(vm, "dy") == doctest::Approx(d.y()));
    CHECK(Number(vm, "dz") == doctest::Approx(d.z()));
    CHECK(Number(vm, "sx") == doctest::Approx(3.0f));
    CHECK(Number(vm, "nLength") == doctest::Approx(1.0f));
    CHECK(Number(vm, "lx") == doctest::Approx(1.0f + (-3.0f) * 0.25f));
    CHECK(Number(vm, "ly") == doctest::Approx(2.0f + (-1.5f) * 0.25f));
    CHECK(Number(vm, "lz") == doctest::Approx(3.0f + 1.0f * 0.25f));
    CHECK(Number(vm, "zeroLength") == 0.0f);
    CHECK(Number(vm, "mx") == doctest::Approx(-1.0f));
    CHECK(Number(vm, "my") == doctest::Approx(10.0f));
    CHECK(vm.GetState()["text"].get<std::string>() == "Vector3(1, 2.5, -3)");
}

TEST_CASE("Bindings - Quat rotates like HedgehogMath")
{
    Fixture  fixture;
    ScriptVM vm(fixture.FileSystem);

    REQUIRE(vm.Run(R"(
        local v = Quat.fromEuler(0, 90, 0) * Vector3(1, 0, 0)
        vx, vy, vz = v.x, v.y, v.z

        local q = Quat.fromEuler(Vector3(10, 20, 30))
        local e = q:euler()
        ex, ey, ez = e.x, e.y, e.z

        local r = (q * q:inverse())
        rw = r.w

        local turn = Quat.axisAngle(Vector3(0, 0, 1), 90)
        local t = turn * Vector3(1, 0, 0)
        tx, ty = t.x, t.y

        local half = Quat.slerp(Quat.identity(), turn, 0.5)
        local h = half * Vector3(1, 0, 0)
        hx, hy = h.x, h.y

        local look = Quat.lookRotation(Vector3(1, 0, 0)) * Vector3(0, 0, -1)
        lookX = look.x
        local lookUp = Quat.lookRotation(Vector3(0, 0, -1), Vector3(1, 0, 0)) * Vector3(0, 1, 0)
        lookUpX = lookUp.x

        assert(Quat() == Quat.identity())
        assert(Quat(0, 0, 0, 2):normalized() == Quat.identity())
        assert(not pcall(function() q.w = 3 end))
        text = tostring(Quat.identity())
    )", "=quat test"));

    const HM::Vector3 expected = HM::Quaternion::FromEuler(0.0f, 90.0f, 0.0f) * HM::Vector3(1.0f, 0.0f, 0.0f);
    CHECK(Number(vm, "vx") == doctest::Approx(expected.x()));
    CHECK(Number(vm, "vy") == doctest::Approx(expected.y()));
    CHECK(Number(vm, "vz") == doctest::Approx(expected.z()));

    CHECK(Number(vm, "ex") == doctest::Approx(10.0f));
    CHECK(Number(vm, "ey") == doctest::Approx(20.0f));
    CHECK(Number(vm, "ez") == doctest::Approx(30.0f));
    CHECK(Number(vm, "rw") == doctest::Approx(1.0f));
    CHECK(Number(vm, "tx") == doctest::Approx(0.0f));
    CHECK(Number(vm, "ty") == doctest::Approx(1.0f));
    CHECK(Number(vm, "hx") == doctest::Approx(0.70710678f));
    CHECK(Number(vm, "hy") == doctest::Approx(0.70710678f));
    CHECK(Number(vm, "lookX") == doctest::Approx(1.0f));
    CHECK(Number(vm, "lookUpX") == doctest::Approx(1.0f));
    CHECK(vm.GetState()["text"].get<std::string>() == "Quat(0, 0, 0, 1)");
}

TEST_CASE("Bindings - a wrong argument is a script error, not a crash")
{
    Fixture  fixture;
    ScriptVM vm(fixture.FileSystem);

    LogCapture capture;
    CHECK_FALSE(vm.Run("local v = Vector3(1, 2, 3) + 5", "=bad add"));
    CHECK_FALSE(vm.Run("local q = Quat.identity() * 2", "=bad multiply"));
    CHECK(vm.Run("ok = true", "=still usable"));
}

TEST_CASE("Bindings - Log.warn gives one Logger warning prefixed with the script file")
{
    Fixture fixture;
    fixture.Write("Scripts/Warner.lua", "local unused = 0\nLog.warn(\"x\")\n");
    ScriptVM vm(fixture.FileSystem);

    LogCapture capture;
    REQUIRE(vm.RunFile("assets://Scripts/Warner.lua"));

    CHECK(capture.CountLines("[WARNING]") == 1);
    CHECK(capture.CountLines("[WARNING][Script] assets://Scripts/Warner.lua:2: x") == 1);
    CHECK(capture.CountLines("[ERROR]") == 0);
}

TEST_CASE("Bindings - print and Log.info are Logger info lines; Log.error is an error")
{
    Fixture fixture;
    fixture.Write("Scripts/Talker.lua",
                  "print(\"hello\", 42, nil, Vector3(1, 2, 3))\n"
                  "Log.info(\"info\")\n"
                  "Log.error(\"bad\", true)\n");
    ScriptVM vm(fixture.FileSystem);

    LogCapture capture;
    REQUIRE(vm.RunFile("assets://Scripts/Talker.lua"));

    CHECK(capture.CountLines("[INFO][Script] assets://Scripts/Talker.lua:1: hello 42 nil Vector3(1, 2, 3)") == 1);
    CHECK(capture.CountLines("[INFO][Script] assets://Scripts/Talker.lua:2: info") == 1);
    CHECK(capture.CountLines("[ERROR][Script] assets://Scripts/Talker.lua:3: bad true") == 1);
    CHECK(capture.CountLines("[WARNING]") == 0);
}

TEST_CASE("Bindings - a failing __tostring in a log call is a script error")
{
    Fixture  fixture;
    ScriptVM vm(fixture.FileSystem);

    LogCapture capture;
    CHECK_FALSE(vm.Run(R"(
        local broken = setmetatable({}, { __tostring = function() error("no text") end })
        Log.info("before", broken)
    )", "=broken tostring"));
    CHECK(vm.GetLastError().find("no text") != std::string::npos);
    CHECK(capture.CountLines("[INFO]") == 0);
    CHECK(vm.Run("Log.info('after')", "=after"));
    CHECK(capture.CountLines("[INFO][Script] after:1: after") == 1);
}
