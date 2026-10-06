-- A plugin DLL the engine's tests load at runtime (HedgehogEngineTest depends on it). Nothing links it.
project "HedgehogTestPlugin"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"

    files { "**.hpp", "**.cpp" }

    includedirs
    {
        "../../ThirdParty",
        "../..",                 -- so "ECS/api/...", "EcsSerialization/api/..." and the like resolve
        "../../HedgehogEngine",  -- so "HedgehogEngine/api/..." resolves
        "%{IncludeDir.yaml_cpp}"
    }

    defines { "YAML_CPP_STATIC_DEFINE" }

    links
    {
        "HedgehogEngine",
        "ECS",
        "EcsSerialization",
        "HedgehogMath",
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
