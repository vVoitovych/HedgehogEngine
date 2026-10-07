IncludeDir = {}
IncludeDir["stb_image"] = "%{wks.location}/ThirdParty/stb"
IncludeDir["yaml_cpp"]  = "%{wks.location}/ThirdParty/YamlCpp/yaml-cpp/include"
IncludeDir["GLFW"]      = "%{wks.location}/ThirdParty/glfw/glfw/include"
IncludeDir["ImGui"]     = "%{wks.location}/ThirdParty/ImGui"
IncludeDir["ImGuiNodeEditor"] = "%{wks.location}/ThirdParty/ImGuiNodeEditor/imgui-node-editor"
IncludeDir["VulkanSDK"] = "%{wks.location}/ThirdParty/vulkan/Include"
IncludeDir["Lua"]       = "%{wks.location}/ThirdParty/Lua/lua"
IncludeDir["sol2"]      = "%{wks.location}/ThirdParty/sol2/sol2/include"
IncludeDir["Tracy"]     = "%{wks.location}/ThirdParty/Tracy/tracy/public"
IncludeDir["miniaudio"] = "%{wks.location}/ThirdParty/miniaudio/miniaudio"
IncludeDir["Jolt"]      = "%{wks.location}/ThirdParty/Jolt/JoltPhysics"

LibraryDir = {}
LibraryDir["VulkanSDK"] = "%{wks.location}/ThirdParty/vulkan"
LibraryDir["yaml_cpp"]  = "%{wks.location}/ThirdParty/yaml-cpp"

Library = {}
Library["VulkanSDK"]        = "%{LibraryDir.VulkanSDK}/vulkan-1.lib"
Library["yaml_cpp_debug"]   = "%{wks.location}/ThirdParty/yaml-cpp/yaml-cppd.lib"
Library["yaml_cpp_release"] = "%{wks.location}/ThirdParty/yaml-cpp/yaml-cpp.lib"

-- Jolt Physics' configuration. Jolt's defines change its classes' layout and inline code, so they
-- must be identical in Jolt.lib and in every project that includes "Jolt/Jolt.h" (Jolt checks part of
-- this at RegisterTypes): both call UseJoltDefines() and nothing else defines a JPH_ macro.
-- CPU features: SSE4.2 with LZCNT/TZCNT and no AVX, so a packaged game runs on any recent x64 CPU.
-- Release defines NDEBUG, which Jolt reads to leave its debug-only checks and members out.
JoltDefines = {
    Common  = { "JPH_USE_SSE4_1", "JPH_USE_SSE4_2", "JPH_USE_LZCNT", "JPH_USE_TZCNT", "JPH_OBJECT_LAYER_BITS=16" },
    Debug   = { "JPH_ENABLE_ASSERTS" },
    Release = { "NDEBUG" },
}

-- Applies JoltDefines to the current project, per configuration; resets the filter afterwards.
function UseJoltDefines()
    defines (JoltDefines.Common)
    filter "configurations:Debug"
        defines (JoltDefines.Debug)
    filter "configurations:Release"
        defines (JoltDefines.Release)
    filter {}
end
