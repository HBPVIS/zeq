
/* Copyright (c) 2017, Human Brain Project
 *                          Stefan.Eilemann@epfl.ch
 */

#include "monitor.h"

#include "detail/constants.h"
#include "detail/socket.h"
#include "log.h"
#include "publisher.h"

#include <zmq.h>

namespace zeroeq
{
class Monitor::Impl
{
public:
    Impl() {}
    virtual ~Impl() {}
    void addSockets(std::vector<zeroeq::detail::Socket>& entries)
    {
        zeroeq::detail::Socket entry;
        entry.socket = _socket;
        entry.events = ZMQ_POLLIN;
        entries.push_back(entry);
    }

    virtual void process(void* socket, Monitor& monitor) = 0;

protected:
    void* _socket;
};

namespace
{
class XPubImpl : public Monitor::Impl
{
public:
    XPubImpl(Sender& sender)
    {
        _socket = sender.getSocket();

        const int on = 1;
        if (zmq_setsockopt(_socket, ZMQ_XPUB_VERBOSE, &on, sizeof(on)) == -1)
        {
            ZEROEQTHROW(std::runtime_error(
                std::string("Enabling ZMQ_XPUB_VERBOSE failed: ") +
                zmq_strerror(zmq_errno())));
        }
    }

    ~XPubImpl()
    {
        if (_socket)
        {
            const int off = 0;
            zmq_setsockopt(_socket, ZMQ_XPUB_VERBOSE, &off, sizeof(off));
        }
    }

    void process(void* socket, Monitor& monitor)
    {
        // Message event is one byte 0=unsub or 1=sub, followed by topic
        zmq_msg_t msg;
        zmq_msg_init(&msg);
        if (zmq_msg_recv(&msg, socket, 0) == -1)
            return;

        const uint8_t* data = (const uint8_t*)zmq_msg_data(&msg);
        switch (*data)
        {
        case 0:
            break; // unsub

        case 1: // sub
            if (zmq_msg_size(&msg) == sizeof(uint8_t) + sizeof(uint128_t) &&
                *(const uint128_t*)(data + 1) == MEERKAT) // new subscriber
            {
                monitor.notifyNewConnection();
            }
            break;

        default:
            ZEROEQWARN << "Unhandled monitor event" << std::endl;
        }
        zmq_msg_close(&msg);
        return;
    }
};

class SocketImpl : public Monitor::Impl
{
public:
    SocketImpl(Sender& sender, void* context)
    {
        const auto inproc = std::string("inproc://zeroeq.monitor.") +
                            servus::make_UUID().getString();

        if (::zmq_socket_monitor(sender.getSocket(), inproc.c_str(),
                                 ZMQ_EVENT_ALL) != 0)
        {
            ZEROEQTHROW(
                std::runtime_error(std::string("Cannot monitor socket: ") +
                                   zmq_strerror(zmq_errno())));
        }

        _socket = ::zmq_socket(context, ZMQ_PAIR);
        if (!_socket)
            ZEROEQTHROW(std::runtime_error(
                std::string("Cannot create inproc socket: ") +
                zmq_strerror(zmq_errno())));

        if (::zmq_connect(_socket, inproc.c_str()) != 0)
        {
            ZEROEQTHROW(std::runtime_error(
                std::string("Cannot connect inproc socket: ") +
                zmq_strerror(zmq_errno())));
        }
    }

    ~SocketImpl()
    {
        if (_socket)
            ::zmq_close(_socket);
    }

    void process(void* socket, Monitor& monitor)
    {
        // Messages consist of 2 Frames, the first containing the event-id and
        // the associated value. The second frame holds the affected endpoint as
        // string.
        zmq_msg_t msg;
        zmq_msg_init(&msg);

        //  The layout of the first Frame is: 16 bit event id 32 bit event value
        if (zmq_msg_recv(&msg, socket, 0) == -1)
            return;

        const uint16_t event = *(uint16_t*)zmq_msg_data(&msg);
        if (!zmq_msg_more(&msg))
            return;
        zmq_msg_close(&msg);

        //  Second frame in message contains event address, skip
        zmq_msg_init(&msg);
        if (zmq_msg_recv(&msg, socket, 0) == -1)
            return;
        zmq_msg_close(&msg);

        switch (event)
        {
        case ZMQ_EVENT_CONNECTED:
        case ZMQ_EVENT_ACCEPTED:
            monitor.notifyNewConnection();
            break;

        default:
            ZEROEQWARN << "Unhandled monitor event " << event << std::endl;
        }
    }
};

Monitor::Impl* newImpl(Sender& sender, void* context)
{
    if (dynamic_cast<Publisher*>(&sender))
        return new XPubImpl(sender);
    return new SocketImpl(sender, context);
}
}

Monitor::Monitor(Sender& sender)
    : Receiver()
    , _impl(newImpl(sender, getZMQContext()))
{
}

Monitor::Monitor(Sender& sender, Receiver& shared)
    : Receiver(shared)
    , _impl(newImpl(sender, getZMQContext()))
{
}

Monitor::~Monitor()
{
}

void Monitor::addSockets(std::vector<zeroeq::detail::Socket>& entries)
{
    _impl->addSockets(entries);
}

void Monitor::process(zeroeq::detail::Socket& socket, uint32_t)
{
    _impl->process(socket.socket, *this);
}
}
