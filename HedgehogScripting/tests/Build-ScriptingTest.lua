project "ScriptingTest"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"

    files { "**.hpp", "**.cpp" }

    includedirs
    {
        "../../ThirdParty",
        "../..",                -- so "HedgehogScripting/api/...", "FileSystem/api/..." resolve
        "../../HedgehogEngine", -- so engine headers' own "HedgehogEngine/api/..." includes resolve
        ".",
        "%{IncludeDir.Lua}",
        "%{IncludeDir.sol2}",
        "%{IncludeDir.yaml_cpp}"
    }

    -- The runtime tests build a scene (SceneManager, the serializer registry) around the runtime.
    defines { "YAML_CPP_STATIC_DEFINE" }

    links
    {
        "HedgehogScripting",
        "HedgehogEngine",
        "EcsSerialization",
        "ECS",
        "FileSystem",
        "HedgehogMath",
        "Logger",
        "Lua",
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
