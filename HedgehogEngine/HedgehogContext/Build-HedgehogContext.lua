project "HedgehogContext"
   kind "StaticLib"
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
        "Scene",
        "imgui",
        "yaml-cpp"
    }


   targetdir ("../../Binaries/" .. OutputDir .. "/%{prj.name}")
   objdir ("../../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

   filter "system:windows"
       systemversion "latest"
       defines {  }

   filter "configurations:Debug"
       defines { "DEBUG" }
       runtime "Debug"
       symbols "On"

   filter "configurations:Release"
       defines { "RELEASE" }
       runtime "Release"
       optimize "On"



