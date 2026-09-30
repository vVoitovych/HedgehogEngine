project "HedgehogScripting"
   kind "StaticLib"
   language "C++"
   cppdialect "C++20"

   files { "api/**.hpp", "src/**.cpp" }

   includedirs
   {
      "..",
      "../HedgehogEngine",
      ".",
      "%{IncludeDir.Lua}",
      "%{IncludeDir.sol2}",
   }

   links {
      "HedgehogEngine",
      "ECS",
      "HedgehogMath",
      "FileSystem",
      "Logger",
      "Lua",
   }

   targetdir (IntermediatesDir)
   objdir    (IntermediatesDir)

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
