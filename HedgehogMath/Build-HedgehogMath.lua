project "HedgehogMath"
   kind "SharedLib"
   language "C++"
   cppdialect "C++20"

   files { "api/**.hpp", "src/**.cpp" }

   includedirs { "." }

   defines { "HEDGEHOG_MATH_EXPORTS" }

   targetdir ("../Binaries/" .. OutputDir .. "/%{prj.name}")
   objdir ("../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

   postbuildcommands
   {
      ("{MKDIR} %{wks.location}Binaries/" .. OutputDir .. "/Client"),
      ("{COPY} %{cfg.buildtarget.abspath} %{wks.location}Binaries/" .. OutputDir .. "/Client/"),
      ("{MKDIR} %{wks.location}Binaries/" .. OutputDir .. "/HedgehogMathTest"),
      ("{COPY} %{cfg.buildtarget.abspath} %{wks.location}Binaries/" .. OutputDir .. "/HedgehogMathTest/")
   }

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
