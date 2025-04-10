project "SPIRV-Reflect"
   kind "StaticLib"
   language "C++"
   cppdialect "C++20"

   staticruntime "off"

    files 
    { 
        "SPIRV-Reflect/spirv_reflect.cpp",
        "SPIRV-Reflect/spirv_reflect.h"
    }

    includedirs
    { 
        "SPIRV-Reflect"
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

