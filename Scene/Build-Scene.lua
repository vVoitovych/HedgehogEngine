project "Scene"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    
    staticruntime "off"

    files    { 
        "**.hpp", "**.cpp"
    }

    includedirs
    {
        "%{IncludeDir.yaml_cpp}",
        ".."
    }

    links {
        "DialogueWindows",
        "ECS",
        "HedgehogMath", 
        "yaml-cpp",
        "Logger"
    }

    targetdir ("../Binaries/" .. OutputDir .. "/%{prj.name}")
    objdir ("../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

    filter "system:windows"
       systemversion "latest"
       defines { }

    filter "configurations:Debug"
       defines { "DEBUG" }
       runtime "Debug"
       symbols "On"

    filter "configurations:Release"
       defines { "RELEASE" }
       runtime "Release"
       optimize "On"

