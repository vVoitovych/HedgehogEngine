-- Jolt Physics (MIT), the engine's physics library, built from the JoltPhysics submodule as a static
-- library. Only HedgehogPhysics links it, and only its src/ includes it (CheckModuleBoundaries rule 6).
project "Jolt"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"

    -- The library alone: no GPU compute backends (DX12, Vulkan, Metal, CPU) or their shaders, which the
    -- engine does not use and which need SDKs and compiled shaders of their own.
    files
    {
        "JoltPhysics/Jolt/**.cpp",
        "JoltPhysics/Jolt/**.h",
        "JoltPhysics/Jolt/**.inl"
    }
    removefiles
    {
        "JoltPhysics/Jolt/Compute/CPU/**",
        "JoltPhysics/Jolt/Compute/DX12/**",
        "JoltPhysics/Jolt/Compute/MTL/**",
        "JoltPhysics/Jolt/Compute/VK/**",
        "JoltPhysics/Jolt/Shaders/**"
    }

    includedirs { "JoltPhysics" }

    -- Third-party code, built as it ships (as yaml-cpp and miniaudio are): its warnings are not ours.
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

    filter {}
    UseJoltDefines()
