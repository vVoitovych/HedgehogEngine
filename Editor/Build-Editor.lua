project "Editor"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++20"

   files { "**.hpp", "**.cpp", "**.rc" }

   includedirs
   {
      ".",
      "..",
      "../HedgehogEngine",
      "../HedgehogEngine/HedgehogEngine/api",
      "../HedgehogEngine/HedgehogRenderer/api",
      "../HedgehogEngine/RHIImGui/api",
      "%{IncludeDir.ImGui}".."/imgui",
      "%{IncludeDir.yaml_cpp}"
   }

   defines { "YAML_CPP_STATIC_DEFINE" }

   links {
      "HedgehogEngine",
      "HedgehogCommon",
      "HedgehogExtract",
      "HedgehogRenderer",
      "RHIImGui",
      "HedgehogWindow",
      "HedgehogSettings",
      "Logger",
      "yaml-cpp",
      "ECS",
      "DialogueWindows",
      "FileSystem",
      "imgui",
      -- Tracy client (linked into HedgehogRenderer) needs these on Windows.
      "ws2_32",
      "dbghelp"
   }

   targetdir ("../Binaries/" .. OutputDir .. "/%{prj.name}")
   objdir ("../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

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
