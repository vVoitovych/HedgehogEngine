project "Lua"
    kind "StaticLib"
    language "C"
    targetdir "Binaries/%{cfg.buildcfg}"
    staticruntime "off"

    files 
    { 
        "lua/*.c",
        "lua/*.h"
    }

    excludes 
    { 
        "lua/onelua.c",
        "lua/lua.c", -- CLI
        "lua/luac.c" -- Compiler
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

