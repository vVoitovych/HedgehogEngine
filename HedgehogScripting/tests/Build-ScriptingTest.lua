project "ScriptingTest"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"

    files { "**.hpp", "**.cpp" }

    includedirs
    {
        "../../ThirdParty",
        "../..",                -- so "HedgehogScripting/...", "FileSystem/..." and "Logger/..." resolve
        "../../HedgehogEngine", -- so "HedgehogEngine/api/..." resolves
        ".",
        "%{IncludeDir.Lua}",
        "%{IncludeDir.sol2}",
        "%{IncludeDir.yaml_cpp}"
    }

    links
    {
        "HedgehogScripting",
        "HedgehogLuaDebug",
        "ws2_32",
        "HedgehogEngine",
        "HedgehogSettings",
        "HedgehogInput",
        "HedgehogAudio",
        "ECS",
        "EcsSerialization",
        "HedgehogMath",
        "FileSystem",
        "Logger",
        "Lua",
        "yaml-cpp"
    }

    targetdir (BinariesDir)
    objdir    (IntermediatesDir)

    filter "system:windows"
        systemversion "latest"
        defines { "YAML_CPP_STATIC_DEFINE" }

    filter "configurations:Debug"
        defines  { "DEBUG" }
        runtime  "Debug"
        symbols  "On"

    filter "configurations:Release"
        defines  { "RELEASE" }
        runtime  "Release"
        optimize "On"
