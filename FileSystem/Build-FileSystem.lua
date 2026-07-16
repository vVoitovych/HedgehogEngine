project "FileSystem"
   kind "SharedLib"
   language "C++"
   cppdialect "C++20"

   files { "api/**.hpp", "src/**.cpp" }

   includedirs { ".", ".." }

   links { "Logger" }

   targetdir ("../Binaries/" .. OutputDir .. "/%{prj.name}")
   objdir ("../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

   postbuildcommands
   {
      ("{MKDIR} %{wks.location}Binaries/" .. OutputDir .. "/Editor"),
      ("{COPY} %{cfg.buildtarget.abspath} %{wks.location}Binaries/" .. OutputDir .. "/Editor/"),
      ("{MKDIR} %{wks.location}Binaries/" .. OutputDir .. "/FileSystemTest"),
      ("{COPY} %{cfg.buildtarget.abspath} %{wks.location}Binaries/" .. OutputDir .. "/FileSystemTest/"),
      ("{MKDIR} %{wks.location}Binaries/" .. OutputDir .. "/EcsSerializationTest"),
      ("{COPY} %{cfg.buildtarget.abspath} %{wks.location}Binaries/" .. OutputDir .. "/EcsSerializationTest/"),
      ("{MKDIR} %{wks.location}Binaries/" .. OutputDir .. "/ContentLoaderTest"),
      ("{COPY} %{cfg.buildtarget.abspath} %{wks.location}Binaries/" .. OutputDir .. "/ContentLoaderTest/")
   }

   filter "system:windows"
      systemversion "latest"
      defines { "FILE_SYSTEM_EXPORT" }

   filter "configurations:Debug"
      defines { "DEBUG" }
      runtime "Debug"
      symbols "On"

   filter "configurations:Release"
      defines { "RELEASE" }
      runtime "Release"
      optimize "On"
