project "HedgehogContext"
   kind "StaticLib"
   language "C++"
   cppdialect "C++20"
   
   staticruntime "off"

    files 
    { 
        "**.hpp", "**.cpp"
    }

    includedirs
    {
        "%{IncludeDir.VulkanSDK}",
        "%{IncludeDir.yaml_cpp}",
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
        "imgui",
        "SPIRV-Reflect",
        "yaml-cpp",
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



