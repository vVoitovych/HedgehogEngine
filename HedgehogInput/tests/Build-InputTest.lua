project "InputTest"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"

    files { "**.hpp", "**.cpp" }

    includedirs
    {
        "../../ThirdParty",
        "../..",           -- so "HedgehogInput/api/..." and "HedgehogEngine/HedgehogWindow/api/..." resolve
        "."
    }

    links
    {
        "HedgehogInput",
        "HedgehogMath",
        "FileSystem",
        "Logger",
        "yaml-cpp"
    }

    defines { "YAML_CPP_STATIC_DEFINE" }

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
