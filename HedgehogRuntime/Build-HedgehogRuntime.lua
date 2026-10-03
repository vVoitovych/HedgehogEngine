project "HedgehogRuntime"
   kind "StaticLib"
   language "C++"
   cppdialect "C++20"

   files { "api/**.hpp", "src/**.hpp", "src/**.cpp" }

   includedirs
   {
      "..",
      "../HedgehogEngine",
      "../HedgehogEngine/HedgehogRenderer/api",
      ".",
      "%{IncludeDir.yaml_cpp}",
      -- HedgehogScripting's ScriptSystem header includes sol2, which includes Lua.
      "%{IncludeDir.Lua}",
      "%{IncludeDir.sol2}"
   }

   -- The editor-free game loop: never ImGui (CheckModuleBoundaries.ps1 rule 4).
   links {
      "HedgehogEngine",
      "HedgehogExtract",
      "HedgehogScripting",
      "HedgehogRenderer",
      "HedgehogInput",
      "HedgehogAudio",
      "HedgehogSettings",
      "HedgehogWindow",
      "FileSystem",
      "Logger"
   }

   targetdir (IntermediatesDir)
   objdir    (IntermediatesDir)

   filter "system:windows"
      systemversion "latest"
      defines { "YAML_CPP_STATIC_DEFINE" }

   filter "configurations:Debug"
      defines { "DEBUG" }
      runtime "Debug"
      symbols "On"

   filter "configurations:Release"
      defines { "RELEASE" }
      runtime "Release"
      optimize "On"
