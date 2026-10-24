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
        "../Docking/WindowPlacement.hpp",
        "../Docking/WindowPlacement.cpp",
        "../EditorSettings.hpp",
        "../EditorSettings.cpp", -- the saved floating places, read and written
        "../Panels/EditorIcons.hpp",
        "../Panels/EditorIconNames.cpp",
        "../Panels/EntityIcon.hpp",
        "../Panels/EntityIcon.cpp",
        "../Panels/MaterialSlots.hpp",
        "../Panels/MaterialSlots.cpp",
        "../Panels/TextSearch.hpp",
        "../Panels/TextSearch.cpp",
        "../Widgets/AxisGizmo.hpp",
        "../Widgets/AxisGizmo.cpp",
        "../Widgets/TransformGizmoMath.hpp",
        "../Widgets/TransformGizmoMath.cpp",
        "../Tools/PluginNameCheck.hpp",
        "../Tools/PluginNameCheck.cpp",
        "../Project/RecentProjects.hpp",
        "../Project/RecentProjects.cpp",
        "../Project/StartupProject.hpp",
        "../Project/StartupProject.cpp",
        "../Tools/NewProjectCheck.hpp",
        "../Tools/NewProjectCheck.cpp",
        "../Tools/GameBinaries.hpp",
        "../Tools/GameBinaries.cpp",
        "../Tools/AnimatorControllerEditor/ControllerEditing.hpp",
        "../Tools/AnimatorControllerEditor/ControllerEditing.cpp",
        "../Tools/AnimatorControllerEditor/ControllerLayout.hpp",
        "../Tools/AnimatorControllerEditor/ControllerLayout.cpp",
        "../Tools/RenderGraphEditor/GraphLayoutFile.hpp",
        "../Tools/RenderGraphEditor/GraphLayoutFile.cpp",
        "../Tools/Profiler/ProfilerAnalysis.hpp",
        "../Tools/Profiler/ProfilerAnalysis.cpp",
        "../Tools/Profiler/ChromeTrace.hpp",
        "../Tools/Profiler/ChromeTrace.cpp"
    }

    includedirs
    {
        "../../ThirdParty",
        "..",              -- so "Docking/..." resolves
        "../..",           -- so "HedgehogMath/api/..." resolves
        "../../HedgehogEngine", -- so "HedgehogEngine/api/..." resolves (MaterialSlots' header-only MaterialData)
        "../../HedgehogEngine/HedgehogRenderer/api", -- the header-only GpuPassTiming the trace writes
        "%{IncludeDir.yaml_cpp}", -- the layout file
        "."
    }

    defines { "YAML_CPP_STATIC_DEFINE" }

    links
    {
        "HedgehogMath",
        "HedgehogSettings",
        "HedgehogAnimation", -- the animator controller model the editing works on
        "yaml-cpp",
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
