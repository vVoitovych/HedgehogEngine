-- The Editor's ImGui-free code, compiled straight from its sources: the executable itself cannot
-- be linked into a test.
project "EditorTest"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"

    files
    {
        "**.hpp",
        "**.cpp",
        "../Docking/DockLayout.hpp",
        "../Docking/DockLayout.cpp"
    }

    includedirs
    {
        "../../ThirdParty",
        "..",              -- so "Docking/..." resolves
        "."
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
