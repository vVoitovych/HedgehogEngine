project "Tracy"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"

    files { "tracy/public/TracyClient.cpp" }

    includedirs { "tracy/public" }

    targetdir ("../../Binaries/" .. OutputDir .. "/%{prj.name}")
    objdir ("../../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        defines { "DEBUG" }
        runtime "Debug"
        symbols "On"

    filter "configurations:Release"
        -- Profiling is Release-only: measurements in Debug are misleading.
        -- TRACY_ON_DEMAND keeps overhead near zero until a Tracy server connects.
        defines { "RELEASE", "TRACY_ENABLE", "TRACY_ON_DEMAND" }
        runtime "Release"
        optimize "On"
