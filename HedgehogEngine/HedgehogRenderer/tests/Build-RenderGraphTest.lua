project "RenderGraphTest"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"

    -- The render graph's declaration side (RGTypes/GraphDescription/GraphBuilder/RGPassBuilder)
    -- has no RHI device, ImGui, or window dependency of its own — only RHI's header-only enums
    -- (RHI::Format etc). Compiling its sources directly here, rather than linking the full
    -- HedgehogRenderer static lib, keeps this test genuinely headless: no Vulkan SDK, no GLFW,
    -- no DLL copying, nothing but doctest.
    files
    {
        "**.hpp", "**.cpp",
        "../api/HedgehogRenderer/Graph/**.hpp",
        "../src/Graph/**.cpp"
    }

    includedirs
    {
        "../../../ThirdParty",
        "../..",   -- so "RHI/api/..." resolves
        "../api",  -- so "HedgehogRenderer/Graph/..." resolves
        "."
    }

    targetdir ("../../../Binaries/" .. OutputDir .. "/%{prj.name}")
    objdir    ("../../../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        defines  { "DEBUG" }
        runtime  "Debug"
        symbols  "On"

    filter "configurations:Release"
        defines  { "RELEASE" }
        runtime  "Release"
        optimize "On"
