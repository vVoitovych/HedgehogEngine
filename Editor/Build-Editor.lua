project "Editor"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++20"

   files { "**.hpp", "**.cpp", "**.rc" }

   includedirs
   {
      "..",
      "../HedgehogEngine",
      "%{IncludeDir.ImGui}".."/imgui",
      "%{IncludeDir.yaml_cpp}"
   }

   defines { "YAML_CPP_STATIC_DEFINE" }

   links {
      "HedgehogContext",
      "HedgehogRenderer",
      "HedgehogWindow",
      "Scene",
      "HedgehogSettings",
      "Logger",
      "yaml-cpp"
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
