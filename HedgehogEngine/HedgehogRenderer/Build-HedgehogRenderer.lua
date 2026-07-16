project "HedgehogRenderer"
   kind "StaticLib"
   language "C++"
   cppdialect "C++20"
   fastuptodate "false"

    files
    {
        "**.hpp", "**.cpp",
        "assets/Shaders/**.vert",
        "assets/Shaders/**.frag",
        "assets/Shaders/**.comp",
        "assets/Shaders/**.glsl",
        "assets/Pipelines/**.pl",
        "assets/VertexDescriptions/**.vdes",
        "assets/Shaders/**.shader"
    }

    defines { "YAML_CPP_STATIC_DEFINE" }

    includedirs
    {
        "%{IncludeDir.VulkanSDK}",
        "%{IncludeDir.ImGui}".."/imgui",
        "%{IncludeDir.yaml_cpp}",
        "%{IncludeDir.Tracy}",
        "../..",
        "..",
        "api",
        "src"
    }

    libdirs
    {
        "%{LibraryDir.VulkanSDK}"
    }

    links {
        "RHI",
        "HedgehogMath",
        "HedgehogEngine",
        "HedgehogSettings",
        "HedgehogWindow",
        "ContentLoader",
        "FileSystem",
        "Logger",
        "imgui",
        "yaml-cpp",
        "Tracy"
    }


   targetdir ("../../Binaries/" .. OutputDir .. "/%{prj.name}")
   objdir ("../../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

   filter "system:windows"
       systemversion "latest"
       defines {  }
       prebuildmessage "Compiling shaders..."
       prebuildcommands {
           'call "%{wks.location}\\ThirdParty\\glslc\\CompileShaders.bat" "%{wks.location}\\HedgehogEngine\\HedgehogRenderer\\assets\\Shaders"'
       }

   filter "configurations:Debug"
       defines { "DEBUG" }
       runtime "Debug"
       symbols "On"

   filter "configurations:Release"
       defines { "RELEASE", "TRACY_ENABLE", "TRACY_ON_DEMAND" }
       runtime "Release"
       optimize "On"



