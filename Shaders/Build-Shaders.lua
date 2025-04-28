project "Shaders"
   kind "StaticLib"
   language "C++"
   cppdialect "C++20"
   targetdir "Binaries/%{cfg.buildcfg}"
   staticruntime "off"

   files 
   { 
        "**.hpp", 
        "**.cpp",
        "**.vert",
        "**.frag",
        "**.glsl"  
    }

   includedirs 
   { 
        "%{IncludeDir.VulkanSDK}",
        ".." 
   } 
   libdirs { "%{LibraryDir.VulkanSDK}" }   

   links 
   { 
       "ContentLoader"
   }

   targetdir ("../Binaries/" .. OutputDir .. "/%{prj.name}")
   objdir ("../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

    filter { "files:**.vert" }
        buildaction "None"

    filter { "files:**.frag" }
        buildaction "None"

    filter { "files:**.glsl" }
        buildaction "None"

   filter "system:windows"
       systemversion "latest"
       defines { }

   filter "configurations:Debug"
       links { "shaderc_combinedd.lib" }
       defines { "DEBUG" }
       runtime "Debug"
       symbols "On"

   filter "configurations:Release"
       links { "shaderc_combined.lib" }
       defines { "RELEASE" }
       runtime "Release"
       optimize "On"
       symbols "On"

