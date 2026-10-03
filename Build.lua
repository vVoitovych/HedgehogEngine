include "Dependencies.lua"

workspace "HedgehogEngine"
   architecture "x64"
   configurations { "Debug", "Release" }
   startproject "Editor"
   staticruntime "off"

   filter "system:windows"
      buildoptions {  }

OutputDir = "%{cfg.system}-%{cfg.architecture}/%{cfg.buildcfg}"

-- Every executable and DLL of a configuration builds into one directory, so each program finds
-- the DLLs it links next to itself with no copy step. Static libraries and object files stay
-- per project under Intermediates: they are build inputs, never loaded at runtime.
BinariesDir      = _MAIN_SCRIPT_DIR .. "/Binaries/" .. OutputDir
IntermediatesDir = _MAIN_SCRIPT_DIR .. "/Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}"
VulkanSDK = os.getenv("VULKAN_SDK")

group "ThirdParty"
   include "ThirdParty/glfw/Build-glfw.lua"
	include "ThirdParty/ImGui/Build-ImGui.lua"
   include "ThirdParty/ImGuiNodeEditor/Build-ImGuiNodeEditor.lua"
   include "ThirdParty/tinyfiledialogs/Build-tinyfiledialogs.lua"
   include "ThirdParty/YamlCpp/Build-YamlCpp.lua"
   include "ThirdParty/Lua/Build-Lua.lua"
   include "ThirdParty/Tracy/Build-Tracy.lua"
   include "ThirdParty/miniaudio/Build-miniaudio.lua"
group ""

include "HedgehogEngine/RHI/Build-RHI.lua"
include "HedgehogEngine/RHIImGui/Build-RHIImGui.lua"

include "HedgehogEngine/HedgehogCommon/Build-HedgehogCommon.lua"
include "HedgehogEngine/HedgehogEngine/Build-HedgehogEngine.lua"
include "HedgehogEngine/HedgehogEngine/tests/Build-HedgehogEngineTest.lua"
include "HedgehogEngine/HedgehogRenderer/Build-HedgehogRenderer.lua"
include "HedgehogEngine/HedgehogRenderer/tests/Build-RenderGraphTest.lua"
include "HedgehogEngine/HedgehogWindow/Build-HedgehogWindow.lua"
include "HedgehogEngine/HedgehogWindow/tests/Build-HedgehogWindowTest.lua"
include "HedgehogEngine/HedgehogSettings/Build-HedgehogSettings.lua"

include "Editor/Build-Editor.lua"
include "Editor/tests/Build-EditorTest.lua"
include "ContentLoader/Build-ContentLoader.lua"
include "ContentLoader/tests/Build-ContentLoaderTest.lua"
include "DialogueWindows/Build-DialogueWindows.lua"
include "ECS/Build-ECS.lua"
include "ECS/tests/Build-ECSTest.lua"
include "EcsSerialization/Build-EcsSerialization.lua"
include "EcsSerialization/tests/Build-EcsSerializationTest.lua"

include "HedgehogMath/Build-HedgehogMath.lua"
include "HedgehogMathTest/Build-HedgehogMathTest.lua"
include "Logger/Build-Logger.lua"
include "FileSystem/Build-FileSystem.lua"
include "FileSystem/tests/Build-FileSystemTest.lua"

include "HedgehogExtract/Build-HedgehogExtract.lua"
include "HedgehogExtract/tests/Build-HedgehogExtractTest.lua"

include "HedgehogScripting/Build-HedgehogScripting.lua"
include "HedgehogScripting/tests/Build-ScriptingTest.lua"

include "HedgehogRuntime/Build-HedgehogRuntime.lua"

include "HedgehogAnimation/Build-HedgehogAnimation.lua"
include "HedgehogAnimation/tests/Build-AnimationTest.lua"

include "HedgehogUI/Build-HedgehogUI.lua"
include "HedgehogUI/tests/Build-UiTest.lua"

include "HedgehogInput/Build-HedgehogInput.lua"
include "HedgehogInput/tests/Build-InputTest.lua"


include "HedgehogAudio/Build-HedgehogAudio.lua"
include "HedgehogAudio/tests/Build-AudioTest.lua"