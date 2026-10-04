project "Cooker"
   kind "ConsoleApp"
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
      "Logger",
      "yaml-cpp"
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
