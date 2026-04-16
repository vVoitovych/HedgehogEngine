project "RHI"
   kind "StaticLib"
   language "C++"
   cppdialect "C++20"

   files
   {
       "**.hpp", "**.cpp"
   }

   includedirs
   {
       -- Own root (so #include "RHI/RHITypes.hpp" resolves from HedgehogEngine/)
       "../..",
       "..",

       -- New submodule-based Vulkan dependencies
       "%{IncludeDir.VulkanHeaders}",
       "%{IncludeDir.Volk}",
       "%{IncludeDir.VMA}",

       -- GLFW (for surface creation in VulkanDevice)
       "%{IncludeDir.GLFW}",
   }

   -- No lib link for Vulkan: Volk loads vulkan-1.dll at runtime.
   links
   {
       "glfw",
       "Logger",
   }

   targetdir ("../../Binaries/" .. OutputDir .. "/%{prj.name}")
   objdir    ("../../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

   filter "system:windows"
       systemversion "latest"

   filter "configurations:Debug"
       defines  { "DEBUG" }
       runtime  "Debug"
       symbols  "On"

   filter "configurations:Release"
       defines  { "RELEASE" }
       runtime  "Release"
       optimize "On"
