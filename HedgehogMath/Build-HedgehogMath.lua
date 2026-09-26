project "HedgehogMath"
   kind "SharedLib"
   language "C++"
   cppdialect "C++20"

   files { "api/**.hpp", "src/**.cpp" }

   includedirs { "." }

   defines { "HEDGEHOG_MATH_EXPORTS" }

   targetdir (BinariesDir)
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
