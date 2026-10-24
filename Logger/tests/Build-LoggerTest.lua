project "LoggerTest"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"

    files { "*.hpp", "*.cpp" }

    includedirs
    {
        "../../ThirdParty",
        "../.."               -- so "Logger/api/..." resolves
    }

    links { "Logger" }

    -- The crash app runs as a child process: each crash it makes ends that process, not the test.
    dependson { "LoggerCrashApp" }

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

project "LoggerCrashApp"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"

    files { "CrashApp/*.cpp" }

    includedirs { "../.." }

    links { "Logger" }

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
