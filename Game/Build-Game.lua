project "Game"
   -- A console in Debug, for the log; a plain window in Release.
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++20"

   files { "**.hpp", "**.cpp", "**.rc" }

   includedirs
   {
      ".",
      "..",
      "../HedgehogEngine",
      "../HedgehogEngine/HedgehogRenderer/api",
      "%{IncludeDir.yaml_cpp}",
      -- HedgehogRuntime's headers reach HedgehogScripting's, which include sol2 and Lua.
      "%{IncludeDir.Lua}",
      "%{IncludeDir.sol2}"
   }

   defines { "YAML_CPP_STATIC_DEFINE" }

   -- The game loop and what it links; never ImGui (CheckModuleBoundaries.ps1 rule 4).
   links {
      "HedgehogRuntime",
      "HedgehogEngine",
      "HedgehogCommon",
      "HedgehogExtract",
      "HedgehogUI",
      "HedgehogInput",
      "HedgehogAudio",
      "HedgehogScripting",
      "HedgehogLuaDebug",
      "Lua",
      "HedgehogRenderer",
      "HedgehogWindow",
      "HedgehogSettings",
      "Logger",
      "yaml-cpp",
      "ECS",
      "EcsSerialization",
      "ContentLoader",
      "FileSystem",
      -- Tracy client (linked into HedgehogRenderer) needs these on Windows.
      "ws2_32",
      "dbghelp"
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
      kind "WindowedApp"
      entrypoint "mainCRTStartup"
      defines { "RELEASE" }
      runtime "Release"
      optimize "On"
