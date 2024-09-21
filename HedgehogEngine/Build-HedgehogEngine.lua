project "HedgehogEngine"
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
        "%{IncludeDir.GLFW}",
        "%{IncludeDir.ImGui}".."/imgui",
        "%{IncludeDir.yaml_cpp}",
        "../HedgehogEngine",
        ".."
    }

    libdirs
    {
        "%{LibraryDir.VulkanSDK}"
    }

    links { 
        "ContentLoader",
        "DialogueWindows",
        "HadgehogMath",
        "Logger",
        "Scene",
        "glfw",
        "imgui",
        "yaml-cpp",
        "vulkan-1"
    }


   targetdir ("../Binaries/" .. OutputDir .. "/%{prj.name}")
   objdir ("../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

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



