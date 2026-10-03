project "miniaudio"
    kind "StaticLib"
    language "C"

    -- miniaudio is a single header; MiniaudioImpl.c compiles its implementation once.
    files
    {
        "MiniaudioImpl.c",
        "miniaudio/miniaudio.h"
    }

    includedirs { "." }

    -- Third-party code, built as it ships (as yaml-cpp is): its own warnings are not ours to fix.
    warnings "Off"

    targetdir (IntermediatesDir)
    objdir    (IntermediatesDir)

    filter "system:windows"
       systemversion "latest"

    filter "configurations:Debug"
       defines { "DEBUG" }
       runtime "Debug"
       symbols "On"

    filter "configurations:Release"
       defines { "RELEASE" }
       runtime "Release"
       optimize "On"
