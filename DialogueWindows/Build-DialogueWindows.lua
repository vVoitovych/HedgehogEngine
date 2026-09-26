project "DialogueWindows"
   kind "SharedLib"
   language "C++"
   cppdialect "C++20"

   files
   {
      "api/**.hpp",
      "src/**.hpp",
      "src/**.cpp"
   }

   includedirs
   {
      ".",             -- allows src/ files to #include "api/..."
      "../ThirdParty"
   }

   defines { "DIALOGUE_WINDOWS_EXPORT" }

   links
   {
      "tinyfiledialogs"
   }

   targetdir (BinariesDir)
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
