project "Editor"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++20"

   files { "**.hpp", "**.cpp", "**.rc" }

   removefiles {
      "EditorGui.hpp",
      "EditorGui.cpp",
      "EditorSettings.hpp",
      "EditorSettings.cpp",
      "Docking/DockTypes.hpp",
      "Docking/DockSystem.hpp",
      "Docking/DockSystem.cpp",
      "Panels/ConsolePanel.hpp",
      "Panels/ConsolePanel.cpp",
      "Tools/PipelineWindow.hpp",
      "Tools/PipelineWindow.cpp",
      "Tools/ShaderWindow.hpp",
      "Tools/ShaderWindow.cpp",
      "Tools/VertexDescriptionWindow.hpp",
      "Tools/VertexDescriptionWindow.cpp",
   }

   includedirs
   {
      "..",
      "../HedgehogEngine",
      "../HedgehogEngine/HedgehogEngine/api",
      "../HedgehogEngine/HedgehogRenderer/api",
      "../HedgehogGui/api",
   }

   defines { "YAML_CPP_STATIC_DEFINE" }

   links {
      "HedgehogEngine",
      "HedgehogRenderer",
      "HedgehogWindow",
      "HedgehogSettings",
      "HedgehogGui",
      "Logger",
      "yaml-cpp",
      "ECS",
      "DialogueWindows"
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
