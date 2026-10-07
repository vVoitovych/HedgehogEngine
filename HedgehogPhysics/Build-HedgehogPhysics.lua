-- The engine's physics: a Jolt world behind headers that never name Jolt (module boundary rule 6).
project "HedgehogPhysics"
   kind "SharedLib"
   language "C++"
   cppdialect "C++20"

   files { "api/**.hpp", "src/**.hpp", "src/**.cpp" }

   -- Jolt is included only from src/: no other module sees it.
   includedirs
   {
      "..",
      "%{IncludeDir.Jolt}",
   }

   links {
      "Jolt",
      "HedgehogMath",
      "Logger",
   }

   targetdir (BinariesDir)
   objdir    (IntermediatesDir)

   filter "system:windows"
       systemversion "latest"
       defines { "HEDGEHOG_PHYSICS_EXPORT" }

   filter "configurations:Debug"
       defines { "DEBUG" }
       runtime "Debug"
       symbols "On"

   filter "configurations:Release"
       defines { "RELEASE" }
       runtime "Release"
       optimize "On"

   -- The same Jolt configuration as Jolt.lib, or its classes would not match.
   filter {}
   UseJoltDefines()
