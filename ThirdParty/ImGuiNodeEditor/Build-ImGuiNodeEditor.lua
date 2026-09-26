project "imgui-node-editor"
   kind "StaticLib"
   language "C++"
   cppdialect "C++20"

   files
   {
      "imgui-node-editor/crude_json.cpp",
      "imgui-node-editor/crude_json.h",
      "imgui-node-editor/imgui_bezier_math.h",
      "imgui-node-editor/imgui_bezier_math.inl",
      "imgui-node-editor/imgui_canvas.cpp",
      "imgui-node-editor/imgui_canvas.h",
      "imgui-node-editor/imgui_extra_math.h",
      "imgui-node-editor/imgui_extra_math.inl",
      "imgui-node-editor/imgui_node_editor.cpp",
      "imgui-node-editor/imgui_node_editor.h",
      "imgui-node-editor/imgui_node_editor_api.cpp",
      "imgui-node-editor/imgui_node_editor_internal.h",
      "imgui-node-editor/imgui_node_editor_internal.inl"
   }

   includedirs
   {
      "%{IncludeDir.ImGui}/imgui",
      "imgui-node-editor"
   }

   links { "imgui" }

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
