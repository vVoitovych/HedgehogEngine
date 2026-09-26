#include "HedgehogRenderer/Graph/GraphAssetLibrary.hpp"

#include "HedgehogRenderer/Graph/GraphAssetParser.hpp"
#include "HedgehogRenderer/Graph/GraphReference.hpp"
#include "HedgehogRenderer/Graph/GraphInstantiator.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace Renderer
{
    namespace
    {
        constexpr const char* GRAPH_EXTENSION = ".graph";

        // Names before file references, each group in string order.
        bool ListedBefore(const std::string& a, const std::string& b)
        {
            const bool aIsFile = ClassifyGraphReference(a) == GraphReferenceKind::File;
            const bool bIsFile = ClassifyGraphReference(b) == GraphReferenceKind::File;
            return aIsFile != bIsFile ? !aIsFile : a < b;
        }

        std::optional<std::string> ReadFile(const std::filesystem::path& file)
        {
            std::ifstream in(file, std::ios::binary);
            if (!in)
                return std::nullopt;
            std::ostringstream content;
            content << in.rdbuf();
            return content.str();
        }

        template<typename ErrorList>
        std::string JoinMessages(const ErrorList& errors)
        {
            std::string joined;
            for (const auto& error : errors)
                joined += "\n  " + error.Message;
            return joined;
        }
    }

    bool GraphAssetLibrary::Register(const std::string& name, const std::filesystem::path& file)
    {
        const auto [position, inserted] = m_Entries.try_emplace(name);
        if (inserted)
            m_Names.insert(std::upper_bound(m_Names.begin(), m_Names.end(), name, ListedBefore), name);

        Entry& entry = position->second;
        entry.File = file;
        std::error_code error;
        entry.LastWriteTime = std::filesystem::last_write_time(file, error);
        return Load(name, entry);
    }

    bool GraphAssetLibrary::RegisterDirectory(const std::filesystem::path& directory)
    {
        std::error_code error;
        if (!std::filesystem::is_directory(directory, error))
            return false;

        m_Directory          = directory;
        m_DirectoryWriteTime = std::filesystem::last_write_time(directory, error);
        RegisterNewDirectoryFiles();
        return true;
    }

    std::vector<std::string> GraphAssetLibrary::RegisterNewDirectoryFiles()
    {
        std::vector<std::string> loaded;
        std::error_code error;
        for (const auto& item : std::filesystem::directory_iterator(m_Directory, error))
        {
            const std::filesystem::path& file = item.path();
            if (!item.is_regular_file(error) || file.extension() != GRAPH_EXTENSION)
                continue;
            const std::string name = file.stem().string();
            if (m_Entries.contains(name))
                continue;
            if (Register(name, file))
            {
                LOGINFO("Registered graph '", name, "' from ", file.string());
                loaded.push_back(name);
            }
        }
        return loaded;
    }

    std::vector<std::string> GraphAssetLibrary::Poll()
    {
        std::vector<std::string> reloaded;
        if (!m_Directory.empty())
        {
            std::error_code error;
            const auto writeTime = std::filesystem::last_write_time(m_Directory, error);
            if (!error && writeTime != m_DirectoryWriteTime)
            {
                m_DirectoryWriteTime = writeTime;
                reloaded = RegisterNewDirectoryFiles();
            }
        }

        for (auto& [name, entry] : m_Entries)
        {
            std::error_code error;
            const auto writeTime = std::filesystem::last_write_time(entry.File, error);
            // A file briefly missing mid-save is not a change; the next Poll() sees the new one.
            if (error || writeTime == entry.LastWriteTime)
                continue;

            entry.LastWriteTime = writeTime;
            if (Load(name, entry))
                reloaded.push_back(name);
        }
        return reloaded;
    }

    const GraphAsset* GraphAssetLibrary::FindOrLoad(std::string_view reference, const VirtualPathResolver& resolveVirtual)
    {
        const std::string key = NormalizeGraphReference(reference);
        if (m_Entries.contains(key) || ClassifyGraphReference(key) == GraphReferenceKind::Name)
            return Find(key);

        // The key is only for lookup; the file keeps the caller's spelling, drive letter included.
        std::optional<std::filesystem::path> file;
        if (!IsVirtualGraphPath(key))
            file = std::filesystem::path(std::string(reference));
        else if (resolveVirtual)
            file = resolveVirtual(key);

        std::error_code error;
        if (!file || !std::filesystem::is_regular_file(*file, error))
        {
            m_MissingFiles[key] = "graph file '" + key + "' does not exist";
            return nullptr;
        }
        m_MissingFiles.erase(key);
        (void)Register(key, *file); // a failed first load is recorded for GetLastError()
        return Find(key);
    }

    const GraphAsset* GraphAssetLibrary::Find(std::string_view reference) const
    {
        const auto it = m_Entries.find(NormalizeGraphReference(reference));
        return it != m_Entries.end() && it->second.KnownGood ? &*it->second.KnownGood : nullptr;
    }

    std::filesystem::path GraphAssetLibrary::GetFile(std::string_view reference) const
    {
        const auto it = m_Entries.find(NormalizeGraphReference(reference));
        return it != m_Entries.end() ? it->second.File : std::filesystem::path{};
    }

    std::string_view GraphAssetLibrary::GetLastError(std::string_view reference) const
    {
        const std::string key = NormalizeGraphReference(reference);
        if (const auto it = m_Entries.find(key); it != m_Entries.end())
            return it->second.LastError;
        const auto missing = m_MissingFiles.find(key);
        return missing != m_MissingFiles.end() ? std::string_view(missing->second) : std::string_view{};
    }

    bool GraphAssetLibrary::Load(const std::string& name, Entry& entry) const
    {
        const std::string label = "graph '" + name + "' (" + entry.File.string() + ")";
        const char* const fallback = entry.KnownGood ? "; keeping the last known-good version"
                                                     : "; no known-good version to fall back to";

        const std::optional<std::string> text = ReadFile(entry.File);
        if (!text)
        {
            entry.LastError = label + ": cannot be read" + fallback;
            LOGERROR(entry.LastError);
            return false;
        }

        const GraphAssetParseResult parsed = GraphAssetParser{}.Parse(*text);
        if (!parsed.Success)
        {
            entry.LastError = label + " is malformed" + fallback + ":" + JoinMessages(parsed.Errors);
            LOGERROR(entry.LastError);
            return false;
        }

        const GraphInstantiationResult validated = GraphInstantiator(m_Registry).Validate(parsed.Asset);
        if (!validated.Success)
        {
            entry.LastError = label + " is invalid" + fallback + ":" + JoinMessages(validated.Errors);
            LOGERROR(entry.LastError);
            return false;
        }

        entry.KnownGood = parsed.Asset;
        entry.LastError.clear();
        return true;
    }
}
