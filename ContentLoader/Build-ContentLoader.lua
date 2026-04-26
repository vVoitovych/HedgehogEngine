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
        "Logger"
    }

    targetdir ("../Binaries/" .. OutputDir .. "/%{prj.name}")
    objdir ("../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

    postbuildcommands
    {
        ("{MKDIR} %{wks.location}Binaries/" .. OutputDir .. "/Editor"),
        ("{COPY} %{cfg.buildtarget.abspath} %{wks.location}Binaries/" .. OutputDir .. "/Editor/")
    }

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
