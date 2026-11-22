-- The engine's navigation: Recast navmesh building and Detour path queries behind headers that never
-- name either (module boundary rule 6).
project "HedgehogNavigation"
   kind "SharedLib"
   language "C++"
   cppdialect "C++20"

   files { "api/**.hpp", "src/**.hpp", "src/**.cpp" }

   -- Recast and Detour are included only from src/: no other module sees them.
   includedirs
   {
      "..",
      "%{IncludeDir.Recast}",
      "%{IncludeDir.Detour}",
   }

   links {
      "RecastNavigation",
      "HedgehogMath",
      "Logger",
   }

   targetdir (BinariesDir)
   objdir    (IntermediatesDir)

   filter "system:windows"
       systemversion "latest"
       defines { "HEDGEHOG_NAVIGATION_EXPORT" }

   filter "configurations:Debug"
       defines { "DEBUG" }
       runtime "Debug"
       symbols "On"

   filter "configurations:Release"
       defines { "RELEASE" }
       runtime "Release"
       optimize "On"
