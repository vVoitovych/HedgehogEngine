project "Shaders"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    fastuptodate "false"

    files 
    { 
        "**.cpp",
        "**.vert",
        "**.frag",
        "**.comp",
        "**.glsl",
        "**.shader"  
    }

   targetdir ("../Binaries/" .. OutputDir .. "/%{prj.name}")
   objdir ("../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

    filter "system:windows"
        systemversion "latest"
        prebuildmessage "Compiling shaders..."
        prebuildcommands {
            'echo Forcing shader build at %time% > "%{wks.location}\\force_prebuild.txt"',
            'call "%{wks.location}\\ThirdParty\\glslc\\CompileShaders.bat" "%{wks.location}\\Shaders\\Shaders"'
        }

   filter "configurations:Debug"
       defines { "DEBUG" }
       runtime "Debug"
       symbols "On"

   filter "configurations:Release"
       defines { "RELEASE" }
       runtime "Release"
       optimize "On"
       symbols "On"

