project "HedgehogExtractTest"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"

    files { "**.hpp", "**.cpp" }

    includedirs
    {
        "../../ThirdParty",
        "../..",              -- so "HedgehogExtract/api/...", "ECS/api/..." resolve
        "../../HedgehogEngine", -- so component headers' own "HedgehogEngine/api/..." includes resolve
        "."
    }

    links
    {
        "HedgehogExtract",
        "HedgehogUI",
        "HedgehogEngine",
        "ECS",
        "HedgehogMath"
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
