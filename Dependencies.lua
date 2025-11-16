-- Dependencies

IncludeDir = {}
IncludeDir["stb_image"] = "%{wks.location}/ThirdParty/stb"
IncludeDir["yaml_cpp"] = "%{wks.location}/ThirdParty/YamlCpp/yaml-cpp/include"
IncludeDir["GLFW"] = "%{wks.location}/ThirdParty/glfw/glfw/include"
IncludeDir["ImGui"] = "%{wks.location}/ThirdParty/ImGui"
IncludeDir["VulkanSDK"] = "%{wks.location}/ThirdParty/vulkan/Include"

LibraryDir = {}
LibraryDir["VulkanSDK"] = "%{wks.location}/ThirdParty/vulkan"
LibraryDir["yaml_cpp"] = "%{wks.location}/ThirdParty/yaml-cpp"

Library = {}
Library["VulkanSDK"] = "%{LibraryDir.VulkanSDK}/vulkan-1.lib"
Library["yaml_cpp_debug"] = "%{wks.location}/ThirdParty/yaml-cpp/yaml-cppd.lib"
Library["yaml_cpp_release"] = "%{wks.location}/ThirdParty/yaml-cpp/yaml-cpp.lib"

