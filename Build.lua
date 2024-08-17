workspace "HadgehogEngine"
   architecture "x64"
   configurations { "Debug", "Release" }
   startproject "Client"

   -- Workspace-wide build options for MSVC
   filter "system:windows"
      buildoptions {  }

OutputDir = "%{cfg.system}-%{cfg.architecture}/%{cfg.buildcfg}"

group "ThirdParty"
	include "ThirdParty/ImGui/Build-ImGui.lua"
   include "ThirdParty/tinyfiledialogs/Build-tinyfiledialogs.lua"
group ""

include "Client/Build-Client.lua"
include "ContentLoader/Build-ContentLoader.lua"
include "DialogueWindows/Build-DialogueWindows.lua"
include "ECS/Build-ECS.lua"
include "HedgehogEngine/Build-HedgehogEngine.lua"
include "HedgehogMath/Build-HedgehogMath.lua"
include "Logger/Build-Logger.lua"
include "Scene/Build-Scene.lua"
include "Shaders/Build-Shaders.lua"