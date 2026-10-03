project "HedgehogInput"
   kind "StaticLib"
   language "C++"
   cppdialect "C++20"

   files { "api/**.hpp", "src/**.hpp", "src/**.cpp" }

   includedirs
   {
      ".",
      "..",
   }

   -- HedgehogWindow's headers only: the RawInput snapshot and its key codes.
   links {
      "HedgehogMath",
   }

   targetdir (IntermediatesDir)
   objdir    (IntermediatesDir)

   filter "system:windows"
       systemversion "latest"

   filter "configurations:Debug"
       defines { "DEBUG" }
       runtime "Debug"
       symbols "On"

   filter "configurations:Release"
       defines { "RELEASE" }
       runtime "Release"
       optimize "On"
