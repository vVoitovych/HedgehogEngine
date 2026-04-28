project "Components"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"

    files { "**.hpp", "**.cpp" }

    includedirs
    {
        "..",
        "%{IncludeDir.yaml_cpp}"
    }

    links {
        "DialogueWindows",
        "ContentLoader",
        "Logger",
        "HedgehogMath",
        "Lua",
        "ECS",
        "yaml-cpp"
    }

    targetdir ("../Binaries/" .. OutputDir .. "/%{prj.name}")
    objdir    ("../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

    defines { "YAML_CPP_STATIC_DEFINE" }

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
