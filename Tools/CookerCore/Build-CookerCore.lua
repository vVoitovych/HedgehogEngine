-- The packaging logic (namespace Cooker): the cook plan, the incremental cook and PackageGame.
-- A static library so Cooker.exe, its tests and the Editor share one implementation. It logs
-- nothing: every problem comes back as text for its caller to show.
project "CookerCore"
   kind "StaticLib"
   language "C++"
   cppdialect "C++20"

   files { "*.hpp", "*.cpp" }

   includedirs
   {
      ".",
      "../..",
      "../../HedgehogEngine",
      "%{IncludeDir.yaml_cpp}"
   }

   defines { "YAML_CPP_STATIC_DEFINE" }

   links {
      "HedgehogEngine",
      "HedgehogSettings",
      "EcsSerialization",
      "FileSystem",
      "yaml-cpp"
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
