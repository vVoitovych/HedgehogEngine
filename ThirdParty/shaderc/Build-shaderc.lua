project "shaderc"
   kind "StaticLib"
   language "C++"
   cppdialect "C++20"
   
   staticruntime "off"

    files 
    { 
        "shaderc/libshaderc/**.cc",
        "shaderc/libshaderc/**.h"
    }

    includedirs
    { 
        "shaderc/libshaderc/include",
        "shaderc/libshaderc_util/include",
        "../SPIRV-Headers/SPIRV-Headers/include"
    }

   targetdir ("../../Binaries/" .. OutputDir .. "/%{prj.name}")
   objdir ("../../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

   filter "system:windows"
       systemversion "latest"
       defines { }

   filter "configurations:Debug"
       defines { "DEBUG" }
       runtime "Debug"
       symbols "On"

   filter "configurations:Release"
       defines { "RELEASE" }
       runtime "Release"
       optimize "On"

