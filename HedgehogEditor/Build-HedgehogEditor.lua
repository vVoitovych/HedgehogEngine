project "HedgehogEditor"
   kind "ConsoleApp"
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
        "%{IncludeDir.ImGui}".."/imgui",
        ".."
    }

    libdirs
    {
        "%{LibraryDir.VulkanSDK}"
    }

    links {         
        "HedgehogContext",
        "imgui",
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



