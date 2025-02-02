#include "messages.h"

#include <libdevcore/logger.h>
#include <libdevcore/str.h>

#include "p2p_socket.h"

namespace c2pool::libnet::messages
{
    std::string string_commands(commands cmd)
    {
        try
        {
            return _string_commands.at(cmd);
        }
        catch (const std::out_of_range &e)
        {
            LOG_WARNING << (int)cmd << " out of range in string_commands";
            return "error";
        }
    }

    commands reverse_string_commands(std::string key)
    {
        try
        {
            return _reverse_string_commands.at(key);
        }
        catch (const std::out_of_range &e)
        {
            LOG_WARNING << key << " out of range in reverse_string_commands";
            return commands::cmd_error;
        }
    }

} // namespace c2pool::libnet::messages