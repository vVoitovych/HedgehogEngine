project "ScriptingTest"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"

    files { "**.hpp", "**.cpp" }

    includedirs
    {
        "../../ThirdParty",
        "../..",                -- so "HedgehogScripting/...", "FileSystem/..." and "Logger/..." resolve
        ".",
        "%{IncludeDir.Lua}",
        "%{IncludeDir.sol2}"
    }

    links
    {
        "HedgehogScripting",
        "FileSystem",
        "Logger",
        "Lua"
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
