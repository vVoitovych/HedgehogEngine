#include "HedgehogRenderer/Graph/GraphAssetLibrary.hpp"

#include "HedgehogRenderer/Graph/GraphAssetParser.hpp"
#include "HedgehogRenderer/Graph/GraphInstantiator.hpp"

#include "Logger/api/Logger.hpp"

#include <fstream>
#include <sstream>

namespace Renderer
{
    namespace
    {
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
        Entry& entry = m_Entries[name];
        entry.File = file;
        std::error_code error;
        entry.LastWriteTime = std::filesystem::last_write_time(file, error);
        return Load(name, entry);
    }

    std::vector<std::string> GraphAssetLibrary::Poll()
    {
        std::vector<std::string> reloaded;
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

    const GraphAsset* GraphAssetLibrary::Find(std::string_view name) const
    {
        const auto it = m_Entries.find(std::string(name));
        return it != m_Entries.end() && it->second.KnownGood ? &*it->second.KnownGood : nullptr;
    }

    std::string_view GraphAssetLibrary::GetLastError(std::string_view name) const
    {
        const auto it = m_Entries.find(std::string(name));
        return it != m_Entries.end() ? std::string_view(it->second.LastError) : std::string_view{};
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
