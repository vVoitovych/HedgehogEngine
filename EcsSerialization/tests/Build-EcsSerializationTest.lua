project "EcsSerializationTest"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"

    files { "**.hpp", "**.cpp" }

    includedirs
    {
        "../../ThirdParty",
        "../..",           -- so "EcsSerialization/api/..." resolves
        ".",
        "%{IncludeDir.yaml_cpp}"
    }

    defines { "YAML_CPP_STATIC_DEFINE" }

    links
    {
        "EcsSerialization",
        "ECS",
        "FileSystem",
        "Logger",
        "HedgehogMath",
        "yaml-cpp"
    }

    targetdir ("../../Binaries/" .. OutputDir .. "/%{prj.name}")
    objdir    ("../../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

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
