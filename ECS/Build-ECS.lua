project "ECS"
   kind "SharedLib"
   language "C++"
   cppdialect "C++20"

   files { "**.h", "**.hpp", "**.cpp" }

   includedirs { }

   targetdir ("../Binaries/" .. OutputDir .. "/%{prj.name}")
   objdir ("../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

   filter "system:windows"
       systemversion "latest"
       defines { "ECS_EXPORT" }
       postbuildcommands {
           "{COPYFILE} %{cfg.targetdir}/ECS.dll ../Binaries/" .. OutputDir .. "/Client/ECS.dll"
       }

   filter "configurations:Debug"
       defines { "DEBUG" }
       runtime "Debug"
       symbols "On"

   filter "configurations:Release"
       defines { "RELEASE" }
       runtime "Release"
       optimize "On"
