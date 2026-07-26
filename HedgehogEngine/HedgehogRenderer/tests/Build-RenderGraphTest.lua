project "RenderGraphTest"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"

    files { "**.hpp", "**.cpp" }

    defines { "YAML_CPP_STATIC_DEFINE" }

    includedirs
    {
        "%{IncludeDir.VulkanSDK}",
        "%{IncludeDir.ImGui}".."/imgui",
        "%{IncludeDir.yaml_cpp}",
        "%{IncludeDir.Tracy}",
        "../../../ThirdParty",   -- doctest
        "../../..",              -- repo root: "FileSystem/api/...", "Logger/api/..."
        "../..",                 -- HedgehogEngine/ dir: "HedgehogCommon/api/...", "HedgehogMath/api/..."
        "../src",                -- HedgehogRenderer/src/...: "RenderGraph/RenderGraphLoader.hpp" etc.
        "."
    }

    links
    {
        "HedgehogRenderer",
        "RHI",
        "HedgehogMath",
        "HedgehogSettings",
        "HedgehogWindow",
        "ContentLoader",
        "FileSystem",
        "Logger",
        "imgui",
        "yaml-cpp",
        "Tracy"
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
        defines  { "RELEASE", "TRACY_ENABLE", "TRACY_ON_DEMAND" }
        runtime  "Release"
        optimize "On"
