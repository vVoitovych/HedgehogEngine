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
        "ECS",
        "EcsSerialization",
        "HedgehogMath",
        "FileSystem"
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
