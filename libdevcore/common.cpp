#include "common.h"
namespace c2pool::dev
{
    time_t timestamp()
    {
        return std::time(nullptr);
    }

    bool ExitSignalHandler::work_status = true;
} // namespace c2pool::time