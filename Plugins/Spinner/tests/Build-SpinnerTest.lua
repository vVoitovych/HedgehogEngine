-- Spinner's tests. They load Spinner.dll through the engine's plugin manager, never linking it.
project "SpinnerTest"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"

    files { "**.hpp", "**.cpp" }

    includedirs
    {
        "../../../ThirdParty",
        "../../..",                 -- so "Plugins/Spinner/...", "ECS/api/..." and the like resolve
        "../../../HedgehogEngine",  -- so "HedgehogEngine/api/..." resolves
        "%{IncludeDir.yaml_cpp}"
    }

    defines { "YAML_CPP_STATIC_DEFINE" }

    links
    {
        "yaml-cpp",
        "HedgehogEngine",
        "HedgehogSettings",
        "ECS",
        "EcsSerialization",
        "HedgehogMath",
        "FileSystem",
        "Logger"
    }

    dependson { "Spinner" }

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