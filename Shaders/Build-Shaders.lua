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
        "**.comp",
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

    filter "system:windows"
        systemversion "latest"
        prebuildmessage "Compiling shaders..."
        prebuildcommands {
            'call "%{wks.location}\\ThirdParty\\glslc\\CompileShaders.bat" "%{wks.location}/Shaders/Shaders"'
        }

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

