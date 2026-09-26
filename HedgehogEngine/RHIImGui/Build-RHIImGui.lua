project "RHIImGui"
   kind "StaticLib"
   language "C++"
   cppdialect "C++20"

   -- ImGui's GPU renderer on top of the RHI, for the application only: RHI links no UI library and
   -- HedgehogRenderer never includes one. It reaches Vulkan through RHI/api/Vulkan/VulkanNative.hpp.
   files { "api/**.hpp", "src/**.cpp" }

   includedirs
   {
      "api",
      "..",
      "%{IncludeDir.VulkanSDK}",
      "%{IncludeDir.ImGui}/imgui"
   }

   links
   {
      "RHI",
      "imgui"
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
