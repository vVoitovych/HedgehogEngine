project "HedgehogAudio"
   kind "SharedLib"
   language "C++"
   cppdialect "C++20"

   files { "api/**.hpp", "src/**.hpp", "src/**.cpp" }

   -- miniaudio.h is included only from src/: no other module sees it.
   includedirs
   {
      "..",
      "%{IncludeDir.miniaudio}",
   }

   links {
      "miniaudio",
      "Logger",
   }

   targetdir (BinariesDir)
   objdir    (IntermediatesDir)

   filter "system:windows"
       systemversion "latest"
       defines { "HEDGEHOG_AUDIO_EXPORT" }

   filter "configurations:Debug"
       defines { "DEBUG" }
       runtime "Debug"
       symbols "On"

   filter "configurations:Release"
       defines { "RELEASE" }
       runtime "Release"
       optimize "On"
