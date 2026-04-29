project "HedgehogEngine"
   kind "SharedLib"
   language "C++"
   cppdialect "C++20"

    files
    {
        "**.hpp", "**.cpp"
    }

    includedirs
    {
        "%{IncludeDir.yaml_cpp}",
        "../..",
        ".."
    }

    links {
        "FrameData",
        "HedgehogCommon",
        "HedgehogSettings",
        "HedgehogWindow",
        "ContentLoader",
        "DialogueWindows",
        "HedgehogMath",
        "Logger",
        "Components",
        "imgui",
        "yaml-cpp"
    }

   targetdir ("../../Binaries/" .. OutputDir .. "/%{prj.name}")
   objdir ("../../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

   postbuildcommands
   {
       ("{MKDIR} %{wks.location}Binaries/" .. OutputDir .. "/Editor"),
       ("{COPY} %{cfg.buildtarget.abspath} %{wks.location}Binaries/" .. OutputDir .. "/Editor/")
   }

   filter "system:windows"
       systemversion "latest"
       defines { "HEDGEHOG_ENGINE_EXPORT" }

   filter "configurations:Debug"
       defines { "DEBUG" }
       runtime "Debug"
       symbols "On"

   filter "configurations:Release"
       defines { "RELEASE" }
       runtime "Release"
       optimize "On"



