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
        "Logger",
        "imgui",
        "yaml-cpp"
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
       defines { "RELEASE" }
       runtime "Release"
       optimize "On"



