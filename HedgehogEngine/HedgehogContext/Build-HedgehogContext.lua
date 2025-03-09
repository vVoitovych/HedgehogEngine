project "HedgehogContext"
   kind "StaticLib"
   language "C++"
   cppdialect "C++20"
   targetdir "Binaries/%{cfg.buildcfg}"
   staticruntime "off"

    files 
    { 
        "**.hpp", "**.cpp"
    }

    includedirs
    {
        "%{IncludeDir.VulkanSDK}",
        "../..",
        ".."
    }

    libdirs
    {
        "%{LibraryDir.VulkanSDK}"
    }

    links { 
        "HedgehogCommon",
        "HedgehogWrappers",
        "ContentLoader",
        "DialogueWindows",
        "HedgehogMath",
        "Logger",
        "Scene",
        "vulkan-1"
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



