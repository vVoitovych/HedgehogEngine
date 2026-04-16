-- Dependencies

IncludeDir = {}
IncludeDir["stb_image"] = "%{wks.location}/ThirdParty/stb"
IncludeDir["yaml_cpp"]  = "%{wks.location}/ThirdParty/YamlCpp/yaml-cpp/include"
IncludeDir["GLFW"]      = "%{wks.location}/ThirdParty/glfw/glfw/include"
IncludeDir["ImGui"]     = "%{wks.location}/ThirdParty/ImGui"

-- Legacy SDK paths (used by HedgehogWrappers until full RHI migration)
IncludeDir["VulkanSDK"] = "%{wks.location}/ThirdParty/vulkan/Include"

-- New submodule-based Vulkan dependencies (used by the RHI module)
IncludeDir["VulkanHeaders"] = "%{wks.location}/ThirdParty/Vulkan-Headers/include"
IncludeDir["Volk"]          = "%{wks.location}/ThirdParty/volk"
IncludeDir["VMA"]           = "%{wks.location}/ThirdParty/VulkanMemoryAllocator/include"

LibraryDir = {}
LibraryDir["VulkanSDK"] = "%{wks.location}/ThirdParty/vulkan"
LibraryDir["yaml_cpp"]  = "%{wks.location}/ThirdParty/yaml-cpp"

Library = {}
Library["VulkanSDK"]        = "%{LibraryDir.VulkanSDK}/vulkan-1.lib"
Library["yaml_cpp_debug"]   = "%{wks.location}/ThirdParty/yaml-cpp/yaml-cppd.lib"
Library["yaml_cpp_release"] = "%{wks.location}/ThirdParty/yaml-cpp/yaml-cpp.lib"

-- Note: the RHI module does NOT link vulkan-1.lib.
-- Volk loads the Vulkan runtime (vulkan-1.dll) dynamically at startup.

