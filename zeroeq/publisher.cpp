
/* Copyright (c) 2014-2017, Human Brain Project
 *                          Daniel Nachbaur <daniel.nachbaur@epfl.ch>
 *                          Stefan.Eilemann@epfl.ch
 */

#include "publisher.h"
#include "detail/application.h"
#include "detail/broker.h"
#include "detail/byteswap.h"
#include "detail/constants.h"
#include "detail/sender.h"
#include "log.h"

#include <servus/serializable.h>
#include <servus/servus.h>

#include <cstring>
#include <map>

namespace zeroeq
{
class Publisher::Impl : public detail::Sender
{
public:
    Impl(const URI& uri_, const std::string& session)
        : detail::Sender(uri_, ZMQ_XPUB)
        , _service(PUBLISHER_SERVICE)
        , _session(session == DEFAULT_SESSION ? getDefaultSession() : session)
    {
        if (session.empty())
            ZEROEQTHROW(std::runtime_error(
                "Empty session is not allowed for publisher"));

        const std::string& zmqURI = buildZmqURI(uri);
        if (zmq_bind(socket, zmqURI.c_str()) == -1)
        {
            zmq_close(socket);
            socket = 0;
            ZEROEQTHROW(std::runtime_error(
                std::string("Cannot bind publisher socket '") + zmqURI + "': " +
                zmq_strerror(zmq_errno())));
        }

        initURI();

        if (session != NULL_SESSION)
            _initService();
    }

    ~Impl() {}
    bool publish(const servus::Serializable& serializable)
    {
        const servus::Serializable::Data& data = serializable.toBinary();
        return publish(serializable.getTypeIdentifier(), data.ptr.get(),
                       data.size);
    }

    bool publish(uint128_t event, const void* data, const size_t size)
    {
#ifdef ZEROEQ_BIGENDIAN
        detail::byteswap(event); // convert to little endian wire protocol
#endif
        const bool hasPayload = data && size > 0;

        zmq_msg_t msgHeader;
        zmq_msg_init_size(&msgHeader, sizeof(event));
        memcpy(zmq_msg_data(&msgHeader), &event, sizeof(event));
        int ret =
            zmq_msg_send(&msgHeader, socket, hasPayload ? ZMQ_SNDMORE : 0);
        zmq_msg_close(&msgHeader);
        if (ret == -1)
        {
            ZEROEQWARN << "Cannot publish message header, got "
                       << zmq_strerror(zmq_errno()) << std::endl;
            return false;
        }

        if (!hasPayload)
            return true;

        zmq_msg_t msg;
        zmq_msg_init_size(&msg, size);
        ::memcpy(zmq_msg_data(&msg), data, size);
        ret = zmq_msg_send(&msg, socket, 0);
        zmq_msg_close(&msg);
        if (ret == -1)
        {
            ZEROEQWARN << "Cannot publish message data, got "
                       << zmq_strerror(zmq_errno()) << std::endl;
            return false;
        }
        return true;
    }

    const std::string& getSession() const { return _session; }
private:
    void _initService()
    {
        if (!servus::Servus::isAvailable())
        {
            ZEROEQTHROW(
                std::runtime_error("No zeroconf implementation available"));
            return;
        }

        _service.set(KEY_INSTANCE, detail::Sender::getUUID().getString());
        _service.set(KEY_USER, getUserName());
        _service.set(KEY_APPLICATION, detail::getApplicationName());
        if (!_session.empty())
            _service.set(KEY_SESSION, _session);

        const servus::Servus::Result& result =
            _service.announce(uri.getPort(), getAddress());

        if (!result)
        {
            ZEROEQTHROW(std::runtime_error("Zeroconf announce failed: " +
                                           result.getString()));
        }
    }

    servus::Servus _service;
    const std::string _session;
};

Publisher::Publisher()
    : _impl(new Impl(URI(), DEFAULT_SESSION))
{
}

Publisher::Publisher(const std::string& session)
    : _impl(new Impl(URI(), session))
{
}

Publisher::Publisher(const URI& uri)
    : _impl(new Impl(uri, DEFAULT_SESSION))
{
}

Publisher::Publisher(const URI& uri, const std::string& session)
    : _impl(new Impl(uri, session))
{
}

Publisher::~Publisher()
{
}

bool Publisher::publish(const servus::Serializable& serializable)
{
    return _impl->publish(serializable);
}

bool Publisher::publish(const uint128_t& event)
{
    return _impl->publish(event, nullptr, 0);
}

bool Publisher::publish(const uint128_t& event, const void* data,
                        const size_t size)
{
    return _impl->publish(event, data, size);
}

std::string Publisher::getAddress() const
{
    return _impl->getAddress();
}

const std::string& Publisher::getSession() const
{
    return _impl->getSession();
}

const URI& Publisher::getURI() const
{
    return _impl->uri;
}

void* Publisher::getSocket()
{
    return _impl->socket;
}
}
