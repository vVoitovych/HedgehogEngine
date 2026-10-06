project "HedgehogEngineTest"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"

    files { "**.hpp", "**.cpp" }

    includedirs
    {
        "../../../ThirdParty",
        "../../..",     -- so "HedgehogMath/api/...", "ECS/api/..." and the like resolve
        "../..",        -- so "HedgehogEngine/api/..." resolves
        ".",
        "%{IncludeDir.yaml_cpp}"
    }

    defines { "YAML_CPP_STATIC_DEFINE" }

    links
    {
        "yaml-cpp",
        "HedgehogEngine",
        "HedgehogInput",
        "HedgehogAudio",
        "HedgehogCommon",
        "HedgehogSettings",
        "ECS",
        "EcsSerialization",
        "HedgehogMath",
        "FileSystem"
    }

    -- The plugin DLLs the tests open at runtime; nothing links them, so they are build dependencies.
    dependson { "HedgehogTestPlugin", "HedgehogTestPluginOldApi" }

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
