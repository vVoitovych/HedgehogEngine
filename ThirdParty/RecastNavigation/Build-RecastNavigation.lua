-- Recast & Detour (zlib), the navigation mesh builder and its runtime, built from the
-- recastnavigation submodule as one static library. Only HedgehogNavigation links it, and only its
-- src/ includes it (CheckModuleBoundaries rule 6).
project "RecastNavigation"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"

    -- Recast and Detour alone: no debug drawing, crowd, tile cache, demo or tests.
    files
    {
        "recastnavigation/Recast/Include/*.h",
        "recastnavigation/Recast/Source/*.cpp",
        "recastnavigation/Detour/Include/*.h",
        "recastnavigation/Detour/Source/*.cpp"
    }

    includedirs
    {
        "recastnavigation/Recast/Include",
        "recastnavigation/Detour/Include"
    }

    -- Third-party code, built as it ships (as Jolt and yaml-cpp are): its warnings are not ours.
    warnings "Off"
    flags { "MultiProcessorCompile" }

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
