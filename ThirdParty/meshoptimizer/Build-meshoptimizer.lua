project "meshoptimizer"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"

    -- The library alone: its demo, glTF tool and JS bindings are not built.
    files
    {
        "meshoptimizer/src/*.cpp",
        "meshoptimizer/src/meshoptimizer.h"
    }

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
