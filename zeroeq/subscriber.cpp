
/* Copyright (c) 2014-2016, Human Brain Project
 *                          Daniel Nachbaur <daniel.nachbaur@epfl.ch>
 *                          Stefan.Eilemann@epfl.ch
 */

#include "subscriber.h"

#include "detail/broker.h"
#include "detail/byteswap.h"
#include "detail/constants.h"
#include "detail/sender.h"
#include "detail/socket.h"
#include "log.h"

#include <servus/serializable.h>
#include <servus/servus.h>

#include <cassert>
#include <cstring>
#include <map>
#include <stdexcept>

namespace zeroeq
{
class Subscriber::Impl
{
public:
    Impl(const std::string& session, void* context)
        : _browser(PUBLISHER_SERVICE)
        , _selfInstance(detail::Sender::getUUID())
        , _session(session == DEFAULT_SESSION ? getDefaultSession() : session)
    {
        if (_session == zeroeq::NULL_SESSION || session.empty())
            ZEROEQTHROW(std::runtime_error(
                std::string("Invalid session name for subscriber")));

        if (!servus::Servus::isAvailable())
            ZEROEQTHROW(
                std::runtime_error(std::string("Empty servus implementation")));

        _browser.beginBrowsing(servus::Servus::IF_ALL);
        update(context);
    }

    Impl(const URI& uri, void* context)
        : _browser(PUBLISHER_SERVICE)
        , _selfInstance(detail::Sender::getUUID())
    {
        if (uri.getHost().empty() || uri.getPort() == 0)
            ZEROEQTHROW(std::runtime_error(
                std::string("Non-fully qualified URI used for subscriber")));

        const std::string& zmqURI = buildZmqURI(uri);
        if (!addConnection(context, zmqURI, uint128_t()))
        {
            ZEROEQTHROW(std::runtime_error("Cannot connect subscriber to " +
                                           zmqURI + ": " +
                                           zmq_strerror(zmq_errno())));
        }
    }

    ~Impl()
    {
        for (const auto& socket : _subscribers)
        {
            if (socket.second)
                zmq_close(socket.second);
        }
        if (_browser.isBrowsing())
            _browser.endBrowsing();
    }

    bool subscribe(servus::Serializable& serializable)
    {
        const auto func = [&serializable](const void* data, const size_t size) {
            serializable.fromBinary(data, size);
        };
        return subscribe(serializable.getTypeIdentifier(), func);
    }

    bool subscribe(const uint128_t& event, const EventPayloadFunc& func)
    {
        if (_eventFuncs.count(event) != 0)
            return false;

        _subscribe(event);
        _eventFuncs[event] = func;
        return true;
    }

    bool unsubscribe(const servus::Serializable& serializable)
    {
        return unsubscribe(serializable.getTypeIdentifier());
    }

    bool unsubscribe(const uint128_t& event)
    {
        if (_eventFuncs.erase(event) == 0)
            return false;

        _unsubscribe(event);
        return true;
    }

    void addSockets(std::vector<detail::Socket>& entries)
    {
        entries.insert(entries.end(), _entries.begin(), _entries.end());
    }

    void process(detail::Socket& socket)
    {
        zmq_msg_t msg;
        zmq_msg_init(&msg);
        zmq_msg_recv(&msg, socket.socket, 0);

        uint128_t type;
        memcpy(&type, zmq_msg_data(&msg), sizeof(type));
#ifndef ZEROEQ_LITTLEENDIAN
        detail::byteswap(type); // convert from little endian wire
#endif
        const bool payload = zmq_msg_more(&msg);
        zmq_msg_close(&msg);

        EventFuncMap::const_iterator i = _eventFuncs.find(type);
        if (i == _eventFuncs.cend())
            ZEROEQTHROW(std::runtime_error("Got unsubscribed event " +
                                           type.getString()));

        if (payload)
        {
            zmq_msg_init(&msg);
            zmq_msg_recv(&msg, socket.socket, 0);
            i->second(zmq_msg_data(&msg), zmq_msg_size(&msg));
            zmq_msg_close(&msg);
        }
        else
            i->second(nullptr, 0);
    }

    void update(void* context)
    {
        if (_browser.isBrowsing())
            _browser.browse(0);
        const servus::Strings& instances = _browser.getInstances();
        for (const std::string& instance : instances)
        {
            const std::string& zmqURI = _getZmqURI(instance);

            // New subscription
            if (_subscribers.count(zmqURI) == 0)
            {
                const std::string& session =
                    _browser.get(instance, KEY_SESSION);
                if (_browser.containsKey(instance, KEY_SESSION) &&
                    !_session.empty() && session != _session)
                {
                    continue;
                }

                const uint128_t identifier(
                    _browser.get(instance, KEY_INSTANCE));
                if (!addConnection(context, zmqURI, identifier))
                {
                    ZEROEQINFO << "Cannot connect subscriber to " << zmqURI
                               << ": " << zmq_strerror(zmq_errno())
                               << std::endl;
                }
            }
        }
    }

    bool addConnection(void* context, const std::string& zmqURI,
                       const uint128_t& instance)
    {
        if (instance == _selfInstance)
            return true;

        _subscribers[zmqURI] = zmq_socket(context, ZMQ_SUB);
        const int hwm = 0;
        zmq_setsockopt(_subscribers[zmqURI], ZMQ_RCVHWM, &hwm, sizeof(hwm));

        if (zmq_connect(_subscribers[zmqURI], zmqURI.c_str()) == -1)
        {
            zmq_close(_subscribers[zmqURI]);
            _subscribers[zmqURI] = 0; // keep empty entry, unconnectable peer
            return false;
        }

        // Tell a Monitor on a Publisher we're here
        if (zmq_setsockopt(_subscribers[zmqURI], ZMQ_SUBSCRIBE, &MEERKAT,
                           sizeof(uint128_t)) == -1)
        {
            ZEROEQTHROW(std::runtime_error(
                std::string("Cannot update meerkat filter: ") +
                zmq_strerror(zmq_errno())));
        }

        // Add existing subscriptions to socket
        for (const auto& i : _eventFuncs)
        {
            if (zmq_setsockopt(_subscribers[zmqURI], ZMQ_SUBSCRIBE, &i.first,
                               sizeof(uint128_t)) == -1)
            {
                ZEROEQTHROW(std::runtime_error(
                    std::string("Cannot update topic filter: ") +
                    zmq_strerror(zmq_errno())));
            }
        }

        assert(_subscribers.find(zmqURI) != _subscribers.end());
        if (_subscribers.find(zmqURI) == _subscribers.end())
            return false;

        detail::Socket entry;
        entry.socket = _subscribers[zmqURI];
        entry.events = ZMQ_POLLIN;
        _entries.push_back(entry);
        ZEROEQINFO << "Subscribed to " << zmqURI << std::endl;
        return true;
    }

    const std::string& getSession() const { return _session; }
private:
    typedef std::map<std::string, void*> SocketMap;
    SocketMap _subscribers;

    typedef std::map<uint128_t, EventPayloadFunc> EventFuncMap;
    EventFuncMap _eventFuncs;

    servus::Servus _browser;
    std::vector<detail::Socket> _entries;

    const uint128_t _selfInstance;
    const std::string _session;

    std::string _getZmqURI(const std::string& instance)
    {
        const size_t pos = instance.find(":");
        const std::string& host = instance.substr(0, pos);
        const std::string& port = instance.substr(pos + 1);

        return buildZmqURI(DEFAULT_SCHEMA, host, std::stoi(port));
    }

    void _subscribe(const uint128_t& event)
    {
        for (const auto& socket : _subscribers)
        {
            if (zmq_setsockopt(socket.second, ZMQ_SUBSCRIBE, &event,
                               sizeof(event)) == -1)
            {
                ZEROEQTHROW(std::runtime_error(
                    std::string("Cannot update topic filter: ") +
                    zmq_strerror(zmq_errno())));
            }
        }
    }

    void _unsubscribe(const uint128_t& event)
    {
        for (const auto& socket : _subscribers)
        {
            if (zmq_setsockopt(socket.second, ZMQ_UNSUBSCRIBE, &event,
                               sizeof(event)) == -1)
            {
                ZEROEQTHROW(std::runtime_error(
                    std::string("Cannot update topic filter: ") +
                    zmq_strerror(zmq_errno())));
            }
        }
    }
};

Subscriber::Subscriber()
    : Receiver()
    , _impl(new Impl(DEFAULT_SESSION, getZMQContext()))
{
}

Subscriber::Subscriber(const std::string& session)
    : Receiver()
    , _impl(new Impl(session, getZMQContext()))
{
}

Subscriber::Subscriber(const URI& uri)
    : Receiver()
    , _impl(new Impl(uri, getZMQContext()))
{
}

Subscriber::Subscriber(Receiver& shared)
    : Receiver(shared)
    , _impl(new Impl(DEFAULT_SESSION, getZMQContext()))
{
}

Subscriber::Subscriber(const std::string& session, Receiver& shared)
    : Receiver(shared)
    , _impl(new Impl(session, getZMQContext()))
{
}

Subscriber::Subscriber(const URI& uri, Receiver& shared)
    : Receiver(shared)
    , _impl(new Impl(uri, getZMQContext()))
{
}

Subscriber::~Subscriber()
{
}

bool Subscriber::subscribe(servus::Serializable& serializable)
{
    return _impl->subscribe(serializable);
}

bool Subscriber::subscribe(const uint128_t& event, const EventFunc& func)
{
    return _impl->subscribe(event, [func](const void*, size_t) { func(); });
}

bool Subscriber::subscribe(const uint128_t& event, const EventPayloadFunc& func)
{
    return _impl->subscribe(event, func);
}

bool Subscriber::unsubscribe(const servus::Serializable& serializable)
{
    return _impl->unsubscribe(serializable);
}

bool Subscriber::unsubscribe(const uint128_t& event)
{
    return _impl->unsubscribe(event);
}

const std::string& Subscriber::getSession() const
{
    return _impl->getSession();
}

void Subscriber::addSockets(std::vector<detail::Socket>& entries)
{
    _impl->addSockets(entries);
}

void Subscriber::process(detail::Socket& socket, const uint32_t)
{
    _impl->process(socket);
}

void Subscriber::update()
{
    _impl->update(getZMQContext());
}

void Subscriber::addConnection(const std::string& uri)
{
    _impl->addConnection(getZMQContext(), uri, uint128_t());
}
}
