
/* Copyright (c) 2016-2017, Human Brain Project
 *                          Stefan.Eilemann@epfl.ch
 *                          Daniel.Nachbaur@epfl.ch
 *                          Raphael.Dumusc@epfl.ch
 */

#ifndef ZEROEQ_HTTP_REQUESTHANDLER_H
#define ZEROEQ_HTTP_REQUESTHANDLER_H

#include <zeroeq/http/request.h>  // member
#include <zeroeq/http/response.h> // member

#include <boost/network/protocol/http/server.hpp>
#include <future>

namespace zeroeq
{
namespace http
{
class RequestHandler;
typedef boost::network::http::server<RequestHandler> HTTPServer;

// HTTP headers for CORS responses
enum class CorsResponseHeader
{
    ACCESS_CONTROL_ALLOW_HEADERS,
    ACCESS_CONTROL_ALLOW_METHODS,
    ACCESS_CONTROL_ALLOW_ORIGIN
};

// Contains in/out values for an HTTP request to exchange information between
// cppnetlib and zeroeq::http::Server
struct Message
{
    // input from cppnetlib and updated in server.cpp
    Request request;

    // input from cppnetlib, internal for CORS requests
    std::string origin;
    std::string accessControlRequestHeaders;
    Method accessControlRequestMethod = Method::ALL;

    // output from zeroeq::http::Server
    std::future<Response> response;

    // output from zeroeq::http::Server, internal for CORS responses
    std::map<CorsResponseHeader, std::string> corsResponseHeaders;
};

// The handler class called for each incoming HTTP request from cppnetlib
class RequestHandler
{
public:
    /**
     * @param zmqURL URL to inproc socket for communication between cppnetlib
     *               thread and zeroeq::http::Server thread
     * @param zmqContext the context to create the inproc socket in
     */
    RequestHandler(const std::string& zmqURL, void* zmqContext);

    ~RequestHandler();

    /** Callback for each request from cppnetlib server. */
    void operator()(const HTTPServer::request& request,
                    HTTPServer::connection_ptr connection);

private:
    void* _socket;
};
}
}

#endif
