project "bc7enc"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"

    -- The encoders and the BC7 decoder alone: its PNG loader and command-line tool are not built.
    -- rgbcx is a single header, compiled once in RgbcxImpl.cpp.
    files
    {
        "bc7enc/bc7enc.c",
        "bc7enc/bc7enc.h",
        "bc7enc/bc7decomp.cpp",
        "bc7enc/bc7decomp.h",
        "bc7enc/rgbcx.h",
        "RgbcxImpl.cpp"
    }

    includedirs { "bc7enc" }

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
