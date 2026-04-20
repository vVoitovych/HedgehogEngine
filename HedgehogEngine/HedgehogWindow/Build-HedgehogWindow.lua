project "HedgehogWindow"
   kind "SharedLib"
   language "C++"
   cppdialect "C++20"

    files
    {
        "api/**.hpp", "src/**.cpp"
    }

    includedirs
    {
        "%{IncludeDir.GLFW}",
        "../..",
        ".."
    }

    links
    {
        "HedgehogMath",
        "Logger",
        "glfw"
    }

   targetdir ("../../Binaries/" .. OutputDir .. "/%{prj.name}")
   objdir ("../../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

   postbuildcommands
   {
      ("{MKDIR} %{wks.location}Binaries/" .. OutputDir .. "/Client"),
      ("{COPY} %{cfg.buildtarget.abspath} %{wks.location}Binaries/" .. OutputDir .. "/Client/")
   }

   filter "system:windows"
       systemversion "latest"
       defines { "HEDGEHOG_WINDOW_EXPORT" }

   filter "configurations:Debug"
       defines { "DEBUG" }
       runtime "Debug"
       symbols "On"

   filter "configurations:Release"
       defines { "RELEASE" }
       runtime "Release"
       optimize "On"
