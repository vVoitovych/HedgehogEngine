#include "Package.hpp"
#include "CookPlan.hpp"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace Cooker
{
    namespace
    {
        constexpr std::array<const char*, 3> FORBIDDEN_NAMES      = { "editor.exe", "cooker.exe", "dialoguewindows.dll" };
        constexpr std::array<const char*, 7> FORBIDDEN_EXTENSIONS = { ".pdb", ".ilk", ".lib", ".vert", ".frag", ".comp", ".glsl" };

        std::string Lower(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return text;
        }

        bool IsForbidden(const std::filesystem::path& file)
        {
            const std::string name      = Lower(file.filename().string());
            const std::string extension = Lower(file.extension().string());
            return std::find(FORBIDDEN_NAMES.begin(), FORBIDDEN_NAMES.end(), name) != FORBIDDEN_NAMES.end() ||
                   std::find(FORBIDDEN_EXTENSIONS.begin(), FORBIDDEN_EXTENSIONS.end(), extension) != FORBIDDEN_EXTENSIONS.end() ||
                   name.ends_with("test.exe") || name.find("imgui") != std::string::npos;
        }

        // Adds sourceDir/name, at the package root, to the plan; a missing file is an error.
        void AddRootFile(CookPlan& plan, const std::filesystem::path& sourceDir, const char* name, const char* prefix,
                         const char* what)
        {
            const std::filesystem::path source = sourceDir / name;
            std::error_code             error;
            if (!std::filesystem::is_regular_file(source, error))
            {
                plan.Errors.push_back(std::string("The ") + what + " " + source.string() + " does not exist.");
                return;
            }
            plan.Files.push_back({ std::string(prefix) + name, source, std::filesystem::path(name) });
        }
    }

    ShaderCompileResult CompileEngineShaders(const std::filesystem::path& engineRoot, const std::filesystem::path& glslc)
    {
        ShaderCompileResult         result;
        const std::filesystem::path shaders = engineRoot / "HedgehogEngine" / "HedgehogRenderer" / "assets" / "Shaders";
        std::error_code             error;
        if (!std::filesystem::is_directory(shaders, error))
            return result;
        const std::filesystem::path compiler = glslc.empty() ? engineRoot / "ThirdParty" / "glslc" / "glslc.exe" : glslc;
        if (!std::filesystem::is_regular_file(compiler, error))
        {
            result.Warnings.push_back("No glslc at " + compiler.string() + "; shaders are packaged as they were last compiled.");
            return result;
        }
        for (const std::string& source : CompileStaleShaders(shaders, compiler, result.Compiled))
            result.Errors.push_back(source + " does not compile.");
        return result;
    }

    std::vector<std::filesystem::path> FindForbiddenPackageFiles(const std::filesystem::path& outDir)
    {
        std::vector<std::filesystem::path> forbidden;
        std::error_code                    error;
        for (auto it = std::filesystem::recursive_directory_iterator(outDir, error);
             !error && it != std::filesystem::recursive_directory_iterator(); it.increment(error))
        {
            if (it->is_regular_file(error) && IsForbidden(it->path()))
                forbidden.push_back(it->path());
        }
        std::sort(forbidden.begin(), forbidden.end());
        return forbidden;
    }

    PackageResult PackageGame(const PackageDesc& desc)
    {
        PackageResult result;

        const ShaderCompileResult shaders = CompileEngineShaders(desc.EngineRoot, desc.Glslc);
        result.ShadersCompiled            = shaders.Compiled;
        result.Warnings                   = shaders.Warnings;
        result.Errors                     = shaders.Errors;
        if (!result.Errors.empty())
            return result;

        // The project's closure and plugins, then what Game.exe needs beside it.
        std::vector<std::string> scenes = desc.ExtraScenes;
        if (desc.AllScenes)
            for (const std::string& scene : ListProjectScenes(desc.ProjectRoot))
                if (std::find(scenes.begin(), scenes.end(), scene) == scenes.end())
                    scenes.push_back(scene);
        CookPlan plan = BuildCookPlan(desc.ProjectRoot, scenes, desc.BinariesDir, desc.EngineRoot);
        for (const char* file : GAME_RUNTIME_FILES)
            AddRootFile(plan, desc.BinariesDir, file, RUNTIME_PATH_PREFIX, "runtime file");
        for (const char* file : LICENCE_FILES)
            AddRootFile(plan, desc.EngineRoot, file, LICENCE_PATH_PREFIX, "licence file");

        const CookResult cooked = CookPackage(plan, desc.OutDir);
        result.Files            = plan.Files.size();
        result.Copied           = cooked.Copied;
        result.Unchanged        = cooked.Unchanged;
        result.Removed          = cooked.Removed;
        result.Errors           = cooked.Errors;
        if (!result.Errors.empty())
            return result;

        for (const std::filesystem::path& file : FindForbiddenPackageFiles(desc.OutDir))
            result.Errors.push_back("The package must not contain " + file.string() + ".");
        return result;
    }
}
