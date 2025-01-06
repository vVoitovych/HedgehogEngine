project "yaml-cpp"
   kind "StaticLib"
   language "C++"
   cppdialect "C++20"
   targetdir "Binaries/%{cfg.buildcfg}"
   staticruntime "off"

   files    { 
        "yaml-cpp/include/**.h",
        "yaml-cpp/src/**.cpp"
    }
   includedirs
   {
    "%{IncludeDir.yaml_cpp}"
   }

   targetdir ("../../Binaries/" .. OutputDir .. "/%{prj.name}")
   objdir ("../../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

    warnings "Off" 

   filter "system:windows"
       systemversion "latest"
       defines { }

   filter "configurations:Debug"
       defines { "DEBUG", "YAML_CPP_STATIC_DEFINE" }
       runtime "Debug"
       symbols "On"

   filter "configurations:Release"
       defines { "RELEASE", "YAML_CPP_STATIC_DEFINE" }
       runtime "Release"
       optimize "On"

