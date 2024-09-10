-- Dependencies

VULKAN_SDK = os.getenv("VULKAN_SDK")

IncludeDir = {}
IncludeDir["stb_image"] = "%{wks.location}/ThirdParty/stb"
IncludeDir["yaml_cpp"] = "%{wks.location}/ThirdParty/yaml-cpp"
IncludeDir["GLFW"] = "%{wks.location}/ThirdParty/GLFW/include"
IncludeDir["ImGui"] = "%{wks.location}/ThirdParty/ImGui"
IncludeDir["VulkanSDK"] = "%{VULKAN_SDK}/Include"

LibraryDir = {}
LibraryDir["VulkanSDK"] = "%{VULKAN_SDK}/Lib"
LibraryDir["yaml_cpp"] = "%{wks.location}/ThirdParty/yaml-cpp"

Library = {}
Library["VulkanSDK"] = "%{LibraryDir.VulkanSDK}/vulkan-1.lib"
Library["yaml_cpp_debug"] = "%{wks.location}/ThirdParty/yaml-cpp/yaml-cppd.lib"
Library["yaml_cpp_release"] = "%{wks.location}/ThirdParty/yaml-cpp/yaml-cpp.lib"

