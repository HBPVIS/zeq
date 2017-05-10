
/* Copyright (c) 2015-2017, Human Brain Project
 *                          Stefan.Eilemann@epfl.ch
 */

#ifndef ZEROEQ_DETAIL_SENDER_H
#define ZEROEQ_DETAIL_SENDER_H

#include <zeroeq/log.h> // ZEROEQINFO
#include <zeroeq/types.h>
#include <zeroeq/uri.h>
#include <zmq.h>

// for NI_MAXHOST
#ifdef _WIN32
#include <Ws2tcpip.h>
#else
#include <netdb.h>
#include <unistd.h>
#endif

#include <stdexcept>

namespace zeroeq
{
namespace detail
{
class Sender
{
    void* _context; // must be private before socket

public:
    Sender(void* context, const int type)
        : Sender(URI(), context, type)
    {
    }

    Sender(const URI& uri_, void* context, const int type)
        : _context(nullptr)
        , uri(uri_)
        , socket(zmq_socket(_createContext(context), type))
    {
        const int hwm = 0;
        zmq_setsockopt(socket, ZMQ_SNDHWM, &hwm, sizeof(hwm));
    }

    ~Sender()
    {
        if (socket)
            zmq_close(socket);
        if (_context)
            zmq_ctx_destroy(_context);
    }

    void* getContext() { return _context; }
    std::string getAddress() const
    {
        return uri.getHost() + ":" + std::to_string(uint32_t(uri.getPort()));
    }

    void initURI()
    {
        std::string host = uri.getHost();
        if (host == "*")
            host.clear();

        uint16_t port = uri.getPort();
        if (host.empty() || port == 0)
        {
            std::string hostStr, portStr;
            _getEndPoint(hostStr, portStr);

            if (port == 0)
            {
                // No overflow is possible unless ZMQ reports bad port number.
                port = std::stoi(portStr);
                uri.setPort(port);
            }

            if (host.empty())
                uri.setHost(hostStr);

            ZEROEQINFO << "Bound to " << uri << std::endl;
        }
    }

    ZEROEQ_API static uint128_t& getUUID();

    URI uri;
    void* socket;

private:
    void _getEndPoint(std::string& host, std::string& port) const
    {
        char buffer[1024];
        size_t size = sizeof(buffer);
        if (zmq_getsockopt(socket, ZMQ_LAST_ENDPOINT, &buffer, &size) == -1)
        {
            ZEROEQTHROW(
                std::runtime_error("Cannot determine port of publisher"));
        }
        const std::string endPoint(buffer);

        port = endPoint.substr(endPoint.find_last_of(":") + 1);
        const size_t start = endPoint.find_last_of("/") + 1;
        const size_t end = endPoint.find_last_of(":");
        host = endPoint.substr(start, end - start);
        if (host == "0.0.0.0")
        {
            char hostname[NI_MAXHOST + 1] = {0};
            gethostname(hostname, NI_MAXHOST);
            hostname[NI_MAXHOST] = '\0';
            host = hostname;
        }
    }

    void* _createContext(void* context)
    {
        if (context)
            return context;

        _context = zmq_ctx_new();
        return _context;
    }
};
}
}

#endif
