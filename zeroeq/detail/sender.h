
/* Copyright (c) 2015-2017, Human Brain Project
 *                          Stefan.Eilemann@epfl.ch
 */

#ifndef ZEROEQ_DETAIL_SENDER_H
#define ZEROEQ_DETAIL_SENDER_H

#include <zeroeq/detail/constants.h>
#include <zeroeq/detail/context.h>
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
    detail::ContextPtr _context; // must be private before socket

public:
    Sender(const URI& uri_, const int type)
        : _context(detail::getContext())
        , uri(uri_)
        , socket(zmq_socket(_context.get(), type))
    {
        const int hwm = 0;
        zmq_setsockopt(socket, ZMQ_SNDHWM, &hwm, sizeof(hwm));
    }

    ~Sender()
    {
        if (socket)
            zmq_close(socket);
    }

    std::string getAddress() const
    {
        return uri.getHost() + ":" + std::to_string(uint32_t(uri.getPort()));
    }

    void initURI()
    {
        if (uri.getScheme() != DEFAULT_SCHEMA)
            return;

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
};
}
}

#endif
