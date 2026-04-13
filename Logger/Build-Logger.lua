project "Logger"
   kind "SharedLib"
   language "C++"
   cppdialect "C++20"

   files { "**.hpp", "**.cpp" }

   includedirs { }

   targetdir ("../Binaries/" .. OutputDir .. "/%{prj.name}")
   objdir ("../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

   filter "system:windows"
       systemversion "latest"
       defines { "LOGGER_EXPORT" }
       postbuildcommands {
           "{COPYFILE} %{cfg.targetdir}/Logger.dll ../Binaries/" .. OutputDir .. "/Client/Logger.dll"
       }

   filter "configurations:Debug"
       defines { "DEBUG" }
       runtime "Debug"
       symbols "On"

   filter "configurations:Release"
       defines { "RELEASE" }
       runtime "Release"
       optimize "On"
