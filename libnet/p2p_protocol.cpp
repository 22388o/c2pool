#include "p2p_protocol.h"
#include "p2p_socket.h"
#include "p2p_node.h"
#include "messages.h"
using namespace c2pool::libnet::messages;

#include <libdevcore/logger.h>

#include <univalue.h>

#include <memory>
using std::shared_ptr, std::weak_ptr, std::make_shared;

namespace c2pool::libnet::p2p
{
    Protocol::Protocol(shared_ptr<c2pool::libnet::p2p::P2PSocket> _sct) : version(3301) //TODO: init version
    {
        LOG_TRACE << "Base protocol: "
                  << "start constuctor";
        _socket = _sct;
    }
}