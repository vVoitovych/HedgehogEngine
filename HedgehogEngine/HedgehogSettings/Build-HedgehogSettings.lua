project "HedgehogSettings"
   kind "SharedLib"
   language "C++"
   cppdialect "C++20"

    files
    {
        "api/**.hpp", "src/**.hpp", "src/**.cpp"
    }

    includedirs
    {
        "%{IncludeDir.yaml_cpp}",
        "../..",
        ".."
    }

    links {
        "FileSystem",
        "Logger",
        "yaml-cpp"
    }

   targetdir (BinariesDir)
   objdir    (IntermediatesDir)

   filter "system:windows"
       systemversion "latest"
       defines { "HEDGEHOG_SETTINGS_EXPORT", "YAML_CPP_STATIC_DEFINE" }

   filter "configurations:Debug"
       defines { "DEBUG" }
       runtime "Debug"
       symbols "On"

   filter "configurations:Release"
       defines { "RELEASE" }
       runtime "Release"
       optimize "On"

