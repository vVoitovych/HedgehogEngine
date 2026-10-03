#pragma once

#include "HedgehogInput/api/InputActionMap.hpp"

#include "HedgehogEngine/HedgehogWindow/api/RawInput.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include <optional>
#include <string>

namespace Editor
{
    // Tools > Input Actions: views and edits assets://Input/actions.yaml, the project's Game and
    // Editor action maps, on a copy saved with HedgehogInput's writer; the engine hot-reloads the file.
    class InputActionsWindow
    {
    public:
        bool Open = false;

        // input is the window's raw input, read while a binding is listening for its key or button.
        void Draw(const FS::FileSystemManager& fileSystem, const HW::RawInput& input);

        // Shows the window with the file read again, as when it is opened from the Content panel.
        void OpenFile(const FS::FileSystemManager& fileSystem);

    private:
        // Which code field of which binding is waiting for the next key or button.
        struct ListenTarget
        {
            bool   Editor   = false; // the Editor map, else the Game map
            size_t Action   = 0;
            size_t Binding  = 0;
            bool   Negative = false; // a key axis's NegativeCode, else Code
        };

        void Load(const FS::FileSystemManager& fileSystem);
        void Save(const FS::FileSystemManager& fileSystem);
        void DrawMap(HInput::InputActionMap& map, bool editor);
        bool DrawBinding(HInput::InputBinding& binding, const ListenTarget& target); // true: remove it
        bool DrawCodeField(const char* label, HInput::BindingSource source, uint16_t& code, const ListenTarget& target);
        void Listen(const HW::RawInput& input);

        // Why the copy cannot be saved (an empty or repeated action name), or nullopt.
        [[nodiscard]] std::optional<std::string> FindProblem() const;

        HInput::InputActionSet       m_Actions;
        bool                         m_Loaded = false;
        bool                         m_Dirty  = false;
        std::string                  m_FileError; // the file on disk did not parse
        std::optional<ListenTarget>  m_Listening;
        HW::RawInput                 m_PreviousInput; // to find the key or button that went down
    };
}
