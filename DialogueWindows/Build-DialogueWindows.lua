project "DialogueWindows"
   kind "SharedLib"
   language "C++"
   cppdialect "C++20"

   files
   {
      "api/**.hpp",
      "src/**.hpp",
      "src/**.cpp"
   }

   includedirs
   {
      ".",             -- allows src/ files to #include "api/..."
      "../ThirdParty"
   }

   defines { "DIALOGUE_WINDOWS_EXPORT" }

   links
   {
      "tinyfiledialogs"
   }

   targetdir ("../Binaries/" .. OutputDir .. "/%{prj.name}")
   objdir ("../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

   -- Copy the DLL next to the Editor executable so it is found at runtime.
   postbuildcommands
   {
      "{MKDIR} %{wks.location}Binaries/%{cfg.system}-%{cfg.architecture}/%{cfg.buildcfg}/Editor",
      "{COPYFILE} %{cfg.buildtarget.abspath} %{wks.location}Binaries/%{cfg.system}-%{cfg.architecture}/%{cfg.buildcfg}/Editor/%{cfg.buildtarget.name}"
   }

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
