project "CookerTest"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"

    -- The cook's logic is the CookerCore library.
    files { "**.hpp", "**.cpp" }

    includedirs
    {
        "../../../ThirdParty",
        "../../..",           -- so "FileSystem/..." and "HedgehogScripting/tests/..." resolve
        "../../../HedgehogEngine",
        "../../CookerCore",
        "%{IncludeDir.yaml_cpp}"
    }

    defines { "YAML_CPP_STATIC_DEFINE" }

    links
    {
        "CookerCore",
        "HedgehogEngine",
        "HedgehogSettings",
        "EcsSerialization",
        "FileSystem",
        "Logger",
        "yaml-cpp"
    }

    targetdir (BinariesDir)
    objdir    (IntermediatesDir)

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
