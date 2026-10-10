project "HedgehogAnimation"
   kind "StaticLib"
   language "C++"
   cppdialect "C++20"

   files { "api/**.hpp", "src/**.hpp", "src/**.cpp" }

   includedirs
   {
      ".",
      "..",
      "%{IncludeDir.yaml_cpp}",
   }

   defines { "YAML_CPP_STATIC_DEFINE" }

   -- ContentLoader's headers only: the converters read its plain-data structs. yaml-cpp reads
   -- and writes animator controller files.
   links {
      "HedgehogMath",
      "yaml-cpp",
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
