project "HedgehogGui"
   kind "StaticLib"
   language "C++"
   cppdialect "C++20"

   files { "**.hpp", "**.cpp" }

   defines { "YAML_CPP_STATIC_DEFINE" }

   includedirs
   {
      "..",                                            -- workspace root (ECS, ContentLoader, Logger, etc.)
      "../HedgehogEngine",                             -- HedgehogEngine/ subdir (RHI/src, HedgehogRenderer/src, etc.)
      "../HedgehogEngine/HedgehogRenderer/api",        -- HedgehogRenderer public headers
      "api",                                           -- HedgehogGui public headers
      "src",                                           -- HedgehogGui internal headers
      "%{IncludeDir.ImGui}".."/imgui",
      "%{IncludeDir.VulkanSDK}",
      "%{IncludeDir.yaml_cpp}",
   }

   links {
      "HedgehogRenderer",
      "HedgehogEngine",
      "HedgehogWindow",
      "HedgehogSettings",
      "HedgehogCommon",
      "RHI",
      "imgui",
      "ECS",
      "ContentLoader",
      "DialogueWindows",
      "Logger",
      "yaml-cpp",
   }

   targetdir ("../Binaries/" .. OutputDir .. "/%{prj.name}")
   objdir    ("../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

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
