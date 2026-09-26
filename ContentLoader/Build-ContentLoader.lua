project "ContentLoader"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"

    files { "api/**.hpp", "src/**.hpp", "src/**.cpp" }

    includedirs
    {
        ".",
        "../ThirdParty",
        ".."
    }

    links {
        "HedgehogMath",
        "Logger",
        "FileSystem"
    }

    targetdir (BinariesDir)
    objdir    (IntermediatesDir)

    filter "system:windows"
       systemversion "latest"
       defines { "CONTENT_LOADER_EXPORT" }

    filter "configurations:Debug"
       defines { "DEBUG" }
       runtime "Debug"
       symbols "On"

    filter "configurations:Release"
       defines { "RELEASE" }
       runtime "Release"
       optimize "On"
