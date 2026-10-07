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
        "../Docking/DockLayout.cpp",
        "../Panels/EditorIcons.hpp",
        "../Panels/EditorIconNames.cpp",
        "../Panels/EntityIcon.hpp",
        "../Panels/EntityIcon.cpp",
        "../Panels/TextSearch.hpp",
        "../Panels/TextSearch.cpp",
        "../Widgets/AxisGizmo.hpp",
        "../Widgets/AxisGizmo.cpp",
        "../Tools/PluginNameCheck.hpp",
        "../Tools/PluginNameCheck.cpp",
        "../Project/RecentProjects.hpp",
        "../Project/RecentProjects.cpp",
        "../Project/StartupProject.hpp",
        "../Project/StartupProject.cpp",
        "../Tools/NewProjectCheck.hpp",
        "../Tools/NewProjectCheck.cpp",
        "../Tools/GameBinaries.hpp",
        "../Tools/GameBinaries.cpp"
    }

    includedirs
    {
        "../../ThirdParty",
        "..",              -- so "Docking/..." resolves
        "../..",           -- so "HedgehogMath/api/..." resolves
        "."
    }

    links
    {
        "HedgehogMath",
        "HedgehogSettings"
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
