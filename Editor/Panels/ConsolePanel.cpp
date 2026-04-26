#include "ConsolePanel.hpp"

#include "imgui.h"

#include <sstream>

namespace Editor
{
    namespace
    {
        LogLevel ClassifyLine(const std::string& line)
        {
            if (line.find("[WARNING]") != std::string::npos) return LogLevel::Warning;
            if (line.find("[ERROR]")   != std::string::npos) return LogLevel::Error;
            if (line.find("[VERBOSE]") != std::string::npos) return LogLevel::Verbose;
            return LogLevel::Info;
        }

        ImVec4 LevelColor(LogLevel level)
        {
            switch (level)
            {
            case LogLevel::Verbose: return ImVec4(0.6f, 0.8f, 0.6f, 1.0f);
            case LogLevel::Warning: return ImVec4(1.0f, 0.9f, 0.0f, 1.0f);
            case LogLevel::Error:   return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
            default:                return ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
            }
        }
    }

    // ─── TeeBuf ──────────────────────────────────────────────────────────────

    ConsolePanel::TeeBuf::TeeBuf(std::streambuf* primary)
        : m_Primary(primary)
    {
    }

    std::string ConsolePanel::TeeBuf::Consume()
    {
        std::string result = m_Captured.str();
        m_Captured.str({});
        m_Captured.clear();
        return result;
    }

    int ConsolePanel::TeeBuf::overflow(int c)
    {
        if (c != EOF)
        {
            m_Primary->sputc(static_cast<char>(c));
            m_Captured.put(static_cast<char>(c));
        }
        return c;
    }

    std::streamsize ConsolePanel::TeeBuf::xsputn(const char* s, std::streamsize n)
    {
        m_Primary->sputn(s, n);
        m_Captured.write(s, n);
        return n;
    }

    // ─── ConsolePanel ────────────────────────────────────────────────────────

    ConsolePanel::ConsolePanel()
        : m_TeeBuf(std::cout.rdbuf())
        , m_OldCoutBuf(std::cout.rdbuf(&m_TeeBuf))
    {
    }

    ConsolePanel::~ConsolePanel()
    {
        std::cout.rdbuf(m_OldCoutBuf);
    }

    void ConsolePanel::Draw()
    {
        ParseCaptured();

        if (ImGui::Button("Clear"))
            Clear();

        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &m_ScrollToBottom);
        ImGui::Separator();

        ImGui::BeginChild("##console_entries", ImVec2(0.0f, 0.0f), false,
            ImGuiWindowFlags_HorizontalScrollbar);

        for (const auto& entry : m_Entries)
            ImGui::TextColored(LevelColor(entry.m_Level), "%s", entry.m_Text.c_str());

        if (m_ScrollToBottom && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 5.0f)
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndChild();
    }

    void ConsolePanel::Clear()
    {
        m_Entries.clear();
    }

    void ConsolePanel::ParseCaptured()
    {
        const std::string raw = m_TeeBuf.Consume();
        if (raw.empty())
            return;

        std::istringstream stream(raw);
        std::string line;
        while (std::getline(stream, line))
        {
            if (!line.empty())
                m_Entries.push_back({ ClassifyLine(line), line });
        }
    }
}
