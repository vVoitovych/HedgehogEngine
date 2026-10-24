// LoggerTest's child process: logs one line, then ends as its argument says (clean, a crash of one
// kind), so the test can see which runs keep their log file.

#include "Logger/api/Logger.hpp"

#include <Windows.h>

#include <cstdlib>
#include <stdexcept>
#include <string>

int main(int argc, char** argv)
{
    // No crash dialog: the test waits for this process to end.
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

    const std::string mode = argc > 1 ? argv[1] : "clean";
    LOGINFO("LoggerCrashApp:", mode);

    if (mode == "access-violation")
    {
        volatile int* nowhere = nullptr;
        *nowhere = 1;
    }
    else if (mode == "abort")
    {
        std::abort();
    }
    else if (mode == "throw")
    {
        throw std::runtime_error("the crash app's exception");
    }
    return 0;
}
