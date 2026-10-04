project "HedgehogScripting"
   kind "StaticLib"
   language "C++"
   cppdialect "C++20"

   files { "api/**.hpp", "src/**.hpp", "src/**.cpp" }

   includedirs
   {
      "..",
      "../HedgehogEngine",
      ".",
      "%{IncludeDir.Lua}",
      "%{IncludeDir.sol2}",
      "%{IncludeDir.yaml_cpp}",
   }

   links {
      "HedgehogEngine",
      "HedgehogInput",
      "HedgehogAudio",
      "ECS",
      "HedgehogMath",
      "FileSystem",
      "Logger",
      "Lua",
      "HedgehogLuaDebug",
      "ws2_32",
      "yaml-cpp",
   }

   targetdir (IntermediatesDir)
   objdir    (IntermediatesDir)

   filter "system:windows"
       systemversion "latest"
       defines { "YAML_CPP_STATIC_DEFINE" }
       -- sol2's usertypes instantiate enough templates to pass MSVC's default section limit.
       buildoptions { "/bigobj" }

   filter "configurations:Debug"
       defines { "DEBUG" }
       runtime "Debug"
       symbols "On"

   filter "configurations:Release"
       defines { "RELEASE" }
       runtime "Release"
       optimize "On"
