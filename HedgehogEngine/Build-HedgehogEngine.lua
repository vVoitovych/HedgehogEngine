project "HedgehogEngine"
   kind "StaticLib"
   language "C++"
   cppdialect "C++20"
   targetdir "Binaries/%{cfg.buildcfg}"
   staticruntime "off"

   files { 
   "Camera/**.hpp", "Camera/**.cpp",
   "Common/**.hpp",
   "Containers/**.hpp", "Containers/**.cpp",
   "Context/**.hpp", "Context/**.cpp",
   "Renderer/**.hpp", "Renderer/**.cpp",
   "RenderPasses/**.hpp", "RenderPasses/**.cpp",
   "RenderQueue/**.hpp", "RenderQueue/**.cpp",
   "ResourceManager/**.hpp", "ResourceManager/**.cpp",
   "WindowManagment/**.hpp", "WindowManagment/**.cpp",
   "Wrappeers/Commands/**.hpp", "Wrappeers/Commands/**.cpp",
   "Wrappeers/Descriptors/**.hpp", "Wrappeers/Descriptors/**.cpp",
   "Wrappeers/Device/**.hpp", "Wrappeers/Device/**.cpp", 
   "Wrappeers/FrameBuffer/**.hpp", "Wrappeers/FrameBuffer/**.cpp", 
   "Wrappeers/Pipeline/**.hpp", "Wrappeers/Pipeline/**.cpp", 
   "Wrappeers/RenderPass/**.hpp", "Wrappeers/RenderPass/**.cpp", 
   "Wrappeers/Resources/Buffer/**.hpp", "Wrappeers/Resources/Buffer/**.cpp", 
   "Wrappeers/Resources/Image/**.hpp", "Wrappeers/Resources/Image/**.cpp", 
   "Wrappeers/Resources/Sampler/**.hpp", "Wrappeers/Resources/Sampler/**.cpp", 
   "Wrappeers/Shaders/**.hpp", "Wrappeers/Shaders/**.cpp", 
   "Wrappeers/SwapChain/**.hpp", "Wrappeers/SwapChain/**.cpp", 
   "Wrappeers/SyncObjects/**.hpp", "Wrappeers/SyncObjects/**.cpp"  
    }

   includedirs
   {
     
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



