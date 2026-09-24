project "RenderGraphTest"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"

    -- The render graph (RGTypes/GraphDescription/GraphBuilder/RGPassBuilder/GraphCompiler/
    -- FrameArena/ResourcePool/RenderGraphRuntime) has no ImGui, GLFW or window dependency of
    -- its own — only RHI's header-only enums/structs (RHI::Format, IRHIDevice/IRHITexture as
    -- pure interfaces — a device is never instantiated here) and Logger. Compiling its sources
    -- directly, rather than linking the full HedgehogRenderer static lib, keeps this test
    -- genuinely headless: no Vulkan SDK, no GLFW, no ImGui, and only one DLL to copy. The graph
    -- asset parser adds yaml-cpp, a static lib, so that stays true.
    files
    {
        "**.hpp", "**.cpp",
        "../api/HedgehogRenderer/Graph/**.hpp",
        "../src/Graph/**.cpp"
    }

    includedirs
    {
        "../../../ThirdParty",
        "../../../",  -- so "Logger/api/..." resolves
        "../..",      -- so "RHI/api/..." resolves
        "../api",     -- so "HedgehogRenderer/Graph/..." resolves
        "%{IncludeDir.yaml_cpp}",
        "."
    }

    defines
    {
        "YAML_CPP_STATIC_DEFINE",
        -- The shipped graph assets, which the equivalence tests load straight from the source tree.
        -- Absolute, resolved at generation time: %{wks.location} would be emitted relative to the
        -- project file, which is not where the test runs from.
        'HH_GRAPH_ASSET_DIR="' .. path.getabsolute("../assets/Graphs") .. '"'
    }

    links { "Logger", "yaml-cpp" }

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
