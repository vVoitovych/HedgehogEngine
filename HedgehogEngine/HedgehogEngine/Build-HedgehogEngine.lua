project "HedgehogEngine"
   kind "SharedLib"
   language "C++"
   cppdialect "C++20"

    files
    {
        "**.hpp", "**.cpp"
    }

    removefiles
    {
        "tests/**.hpp", "tests/**.cpp"
    }

    includedirs
    {
        "%{IncludeDir.yaml_cpp}",
        "../..",
        ".."
    }

    links {
        "HedgehogCommon",
        "HedgehogSettings",
        "HedgehogWindow",
        "ContentLoader",
        "HedgehogAnimation",
        "HedgehogInput",
        "HedgehogAudio",
        "HedgehogMath",
        "Logger",
        "ECS",
        "EcsSerialization",
        "FileSystem",
        "yaml-cpp"
    }

   targetdir (BinariesDir)
   objdir    (IntermediatesDir)

   filter "system:windows"
       systemversion "latest"
       defines { "HEDGEHOG_ENGINE_EXPORT", "YAML_CPP_STATIC_DEFINE" }

   filter "configurations:Debug"
       defines { "DEBUG" }
       runtime "Debug"
       symbols "On"

   filter "configurations:Release"
       defines { "RELEASE" }
       runtime "Release"
       optimize "On"

