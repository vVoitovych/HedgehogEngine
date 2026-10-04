project "CookerTest"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"

    -- The cook's logic compiled in directly: the Cooker is an executable.
    files { "**.hpp", "**.cpp", "../CookPlan.hpp", "../CookPlan.cpp" }

    includedirs
    {
        "../../../ThirdParty",
        "../../..",           -- so "FileSystem/..." and "HedgehogScripting/tests/..." resolve
        "../../../HedgehogEngine",
        "..",
        "%{IncludeDir.yaml_cpp}"
    }

    defines { "YAML_CPP_STATIC_DEFINE" }

    links
    {
        "HedgehogEngine",
        "HedgehogSettings",
        "EcsSerialization",
        "FileSystem",
        "Logger",
        "yaml-cpp"
    }

    targetdir (BinariesDir)
    objdir    (IntermediatesDir)

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        defines  { "DEBUG" }
        runtime  "Debug"
        symbols  "On"

    filter "configurations:Release"
        defines  { "RELEASE" }
        runtime  "Release"
        optimize "On"
