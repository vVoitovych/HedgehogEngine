project "FileSystem"
   kind "SharedLib"
   language "C++"
   cppdialect "C++20"

   files { "api/**.hpp", "src/**.cpp" }

   includedirs { ".", ".." }

   links { "Logger" }

   targetdir (BinariesDir)
   objdir    (IntermediatesDir)

   filter "system:windows"
      systemversion "latest"
      defines { "FILE_SYSTEM_EXPORT" }

   filter "configurations:Debug"
      defines { "DEBUG" }
      runtime "Debug"
      symbols "On"

   filter "configurations:Release"
      defines { "RELEASE" }
      runtime "Release"
      optimize "On"
