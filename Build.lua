include "Dependencies.lua"

workspace "HedgehogEngine"
   architecture "x64"
   configurations { "Debug", "Release" }
   startproject "Client"

   -- Workspace-wide build options for MSVC
   filter "system:windows"
      buildoptions {  }

OutputDir = "%{cfg.system}-%{cfg.architecture}/%{cfg.buildcfg}"
VulkanSDK = os.getenv("VULKAN_SDK")

group "ThirdParty"
   include "ThirdParty/glfw/Build-glfw.lua"
	include "ThirdParty/ImGui/Build-ImGui.lua"
   include "ThirdParty/tinyfiledialogs/Build-tinyfiledialogs.lua"
   include "ThirdParty/YamlCpp/Build-YamlCpp.lua"
group ""

include "HedgehogEngine/HedgehogCommon/Build-HedgehogCommon.lua"
include "HedgehogEngine/HedgehogContext/Build-HedgehogContext.lua"
include "HedgehogEngine/HedgehogRenderer/Build-HedgehogRenderer.lua"
include "HedgehogEngine/HedgehogWrappers/Build-HedgehogWrappers.lua"
include "HedgehogEngine/HedgehogSettings/Build-HedgehogSettings.lua"

include "Client/Build-Client.lua"
include "ContentLoader/Build-ContentLoader.lua"
include "DialogueWindows/Build-DialogueWindows.lua"
include "ECS/Build-ECS.lua"


include "HedgehogMath/Build-HedgehogMath.lua"
include "Logger/Build-Logger.lua"
include "Scene/Build-Scene.lua"
include "Shaders/Build-Shaders.lua"