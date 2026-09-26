project "HedgehogCommon"
   kind "SharedLib"
   language "C++"
   cppdialect "C++20"

   files { "api/**.hpp", "src/**.cpp" }

   includedirs
   {
      ".",
      "../.."
   }

   links
   {
      "HedgehogMath"
   }

   targetdir (BinariesDir)
   objdir    (IntermediatesDir)

   filter "system:windows"
       systemversion "latest"
       defines { "HEDGEHOG_COMMON_EXPORT" }

   filter "configurations:Debug"
       defines { "DEBUG" }
       runtime "Debug"
       symbols "On"

   filter "configurations:Release"
       defines { "RELEASE" }
       runtime "Release"
       optimize "On"
