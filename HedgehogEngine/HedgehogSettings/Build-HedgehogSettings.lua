project "HedgehogSettings"
   kind "SharedLib"
   language "C++"
   cppdialect "C++20"

    files
    {
        "api/**.hpp", "src/**.cpp"
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

   targetdir ("../../Binaries/" .. OutputDir .. "/%{prj.name}")
   objdir ("../../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

   postbuildcommands
   {
       ("{MKDIR} %{wks.location}Binaries/" .. OutputDir .. "/Editor"),
       ("{COPY} %{cfg.buildtarget.abspath} %{wks.location}Binaries/" .. OutputDir .. "/Editor/"),
       ("{MKDIR} %{wks.location}Binaries/" .. OutputDir .. "/EcsSerializationTest"),
       ("{COPY} %{cfg.buildtarget.abspath} %{wks.location}Binaries/" .. OutputDir .. "/EcsSerializationTest/"),
       ("{MKDIR} %{wks.location}Binaries/" .. OutputDir .. "/HedgehogExtractTest"),
       ("{COPY} %{cfg.buildtarget.abspath} %{wks.location}Binaries/" .. OutputDir .. "/HedgehogExtractTest/")
   }

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



