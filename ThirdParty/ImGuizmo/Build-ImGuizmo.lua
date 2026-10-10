-- ImGuizmo (MIT, ThirdParty/ImGuizmo/ImGuizmo, a submodule pinned to the vVoitovych fork): the
-- move, rotate and scale gizmos the Editor draws over its Scene view. Only ImGuizmo itself is built,
-- not the repository's other widgets (sequencer, curve and graph editors).
project "ImGuizmo"
   kind "StaticLib"
   language "C++"
   cppdialect "C++20"

   files
   {
      "ImGuizmo/src/ImGuizmo.cpp",
      "ImGuizmo/src/ImGuizmo.h"
   }

   includedirs
   {
      "%{IncludeDir.ImGui}/imgui",
      "ImGuizmo/src"
   }

   links { "imgui" }

   -- Third-party code, built as it ships (as yaml-cpp and miniaudio are): its warnings are not ours.
   warnings "Off"

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
