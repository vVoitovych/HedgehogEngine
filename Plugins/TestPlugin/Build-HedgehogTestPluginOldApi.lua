-- The test plugin built as if for an older engine: it reports plugin API version 0, so the engine
-- must refuse it. Same source as HedgehogTestPlugin; HedgehogEngineTest depends on it too.
project "HedgehogTestPluginOldApi"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"

    files { "TestPlugin.cpp" }

    includedirs
    {
        "../../ThirdParty",
        "../..",                 -- so "ECS/api/...", "EcsSerialization/api/..." and the like resolve
        "../../HedgehogEngine",  -- so "HedgehogEngine/api/..." resolves
        "%{IncludeDir.yaml_cpp}"
    }

    defines { "YAML_CPP_STATIC_DEFINE", "HH_TEST_PLUGIN_OLD_API" }

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
