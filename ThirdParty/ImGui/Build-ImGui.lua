project "imgui"
   kind "StaticLib"
   language "C++"
   cppdialect "C++20"
   targetdir "Binaries/%{cfg.buildcfg}"
   staticruntime "off"

   files 
   { 
    "imgui/imconfig.h", 
    "imgui/imgui.cpp",
    "imgui/imgui.h",
    "imgui/imgui_demo.cpp",
    "imgui/imgui_draw.cpp",
    "imgui/imgui_internal.h",
    "imgui/imgui_tables.cpp",
    "imgui/imgui_widgets.cpp",
    "imgui/imstb_rectpack.h",
    "imgui/imstb_textedit.h",
    "imgui/imstb_truetype.h",
    "imgui/backends/imgui_impl_glfw.h",
    "imgui/backends/imgui_impl_glfw.cpp",
    "imgui/backends/imgui_impl_vulkan.h",
    "imgui/backends/imgui_impl_vulkan.cpp" 
    }

   includedirs
   { 
    "%{IncludeDir.GLFW}",
    "%{IncludeDir.VulkanSDK}",
    "imgui" 
   }

   targetdir ("../../Binaries/" .. OutputDir .. "/%{prj.name}")
   objdir ("../../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

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
       symbols "On"

