project "HedgehogSettingsTest"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"

    files { "**.hpp", "**.cpp" }

    includedirs
    {
        "../../../ThirdParty",
        "../../..",           -- so "HedgehogEngine/HedgehogSettings/api/..." and "FileSystem/..." resolve
        "../..",              -- so the settings sources' own "HedgehogSettings/api/..." resolve
        "."
    }

    links
    {
        "HedgehogSettings",
        "FileSystem",
        "Logger"
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
