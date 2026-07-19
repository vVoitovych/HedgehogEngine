project "ContentLoaderTest"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"

    files { "**.hpp", "**.cpp" }

    includedirs
    {
        "../../ThirdParty",
        "../..",           -- so "ContentLoader/api/..." resolves
        "."
    }

    links
    {
        "ContentLoader",
        "FileSystem",
        "Logger",
        "HedgehogMath"
    }

    targetdir ("../../Binaries/" .. OutputDir .. "/%{prj.name}")
    objdir    ("../../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

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
