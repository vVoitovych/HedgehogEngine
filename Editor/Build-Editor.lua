project "Editor"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++20"

   files { "**.hpp", "**.cpp", "**.rc" }
   removefiles { "tests/**" }

   includedirs
   {
      ".",
      "..",
      "../HedgehogEngine",
      "../HedgehogEngine/HedgehogEngine/api",
      "../HedgehogEngine/HedgehogRenderer/api",
      "../HedgehogEngine/RHIImGui/api",
      "%{IncludeDir.ImGui}".."/imgui",
      "%{IncludeDir.ImGuiNodeEditor}",
      "%{IncludeDir.yaml_cpp}",
      -- HedgehogScripting's ScriptSystem header includes sol2, which includes Lua.
      "%{IncludeDir.Lua}",
      "%{IncludeDir.sol2}"
   }

   defines { "YAML_CPP_STATIC_DEFINE" }

   links {
      "HedgehogEngine",
      "HedgehogCommon",
      "HedgehogExtract",
      "HedgehogUI",
      "HedgehogInput",
      "HedgehogAudio",
      "HedgehogScripting",
      "HedgehogLuaDebug",
      "HedgehogRuntime",
      "Lua",
      "HedgehogRenderer",
      "RHIImGui",
      "HedgehogWindow",
      "HedgehogSettings",
      "Logger",
      "yaml-cpp",
      "ECS",
      "EcsSerialization",
      "DialogueWindows",
      "ContentLoader",
      "FileSystem",
      "imgui",
      "imgui-node-editor",
      -- Tracy client (linked into HedgehogRenderer) needs these on Windows.
      "ws2_32",
      "dbghelp"
   }

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
