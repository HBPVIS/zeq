
/* Copyright (c) 2016, Human Brain Project
 *                     Stefan.Eilemann@epfl.ch
 *                     Daniel.Nachbaur@epfl.ch
 */

#ifndef ZEROEQ_HTTP_REQUESTHANDLER_H
#define ZEROEQ_HTTP_REQUESTHANDLER_H

#include <zeroeq/log.h>
#include <zeroeq/uri.h>
#include <boost/network/protocol/http/server.hpp>
#include <string>
#include <zmq.h>

namespace zeroeq
{
namespace http
{

class RequestHandler;
typedef boost::network::http::server< RequestHandler > HTTPServer;

struct HTTPRequest
{
    enum class Method
    {
        PUT,
        GET
    };

    Method method;
    std::string url;
    std::string request;
    HTTPServer::response::status_type status;
    std::string reply;
};

class RequestHandler
{
public:
    RequestHandler( const std::string& zmqURL, void* zmqContext )
        : _socket( zmq_socket( zmqContext, ZMQ_PAIR ))
    {
        if( zmq_connect( _socket, zmqURL.c_str( )) == -1 )
        {
            ZEROEQTHROW( std::runtime_error(
                            "Cannot connect RequestHandler to inproc socket" ));
        }
    }

    ~RequestHandler()
    {
        zmq_close( _socket );
    }

    void operator() ( const HTTPServer::request& request,
                      HTTPServer::response& response )
    {
        const std::string& method = request.method;

        HTTPRequest httpRequest;
        if( method == "PUT" )
            httpRequest.method = HTTPRequest::Method::PUT;
        else if( method == "GET" )
            httpRequest.method = HTTPRequest::Method::GET;
        else
        {
            response = HTTPServer::response::stock_reply(
                        HTTPServer::response::method_not_allowed );
            _addCorsHeaders( response );
            return;
        }

        httpRequest.url = URI( request.destination ).getPath();
        httpRequest.request = request.body;
        void* httpRequestPtr = &httpRequest;
        zmq_send( _socket, &httpRequestPtr, sizeof(void*), 0 );
        bool done;
        zmq_recv( _socket, &done, sizeof( done ), 0 );

        const bool haveReply = !httpRequest.reply.empty();
        if( !haveReply )
        {
            if( httpRequest.status == HTTPServer::response::ok )
                response.status = HTTPServer::response::ok;
            else
                response =
                        HTTPServer::response::stock_reply( httpRequest.status );

            _addCorsHeaders( response );
            return;
        }

        response.status = httpRequest.status;
        response.content = httpRequest.reply;

        // don't use stock_reply() here as we need a different Content-Type
        HTTPServer::response_header contentLength;
        contentLength.name = "Content-Length";
        contentLength.value = std::to_string( httpRequest.reply.length());

        HTTPServer::response_header contentType;
        contentType.name = "Content-Type";
        contentType.value = "application/json";

        response.headers.push_back( contentLength );
        response.headers.push_back( contentType );
        _addCorsHeaders( response );
    }

    void log( const HTTPServer::string_type& )
    {
        // Intentionally empty as only reports "Bad file descriptor" during
        // unit tests where the server closes too fast w/o any connected client.
        // Needs to be 'implemented' though to make cppnetlib happy.
    }

private:
    void _addCorsHeaders( HTTPServer::response& response )
    {
        // In a typical situation, user agents can discover via a preflight
        // request whether a cross-origin resource is prepared to accept
        // requests. The current implementation of this class does not support
        // this mechanism and has to accept cross-origin resources. In order to
        // achieve that, access control headers are added to all HTTP responses,
        // meaning that all sources are accepted for all requests. More
        // information can be found here: https://www.w3.org/TR/cors

        HTTPServer::response_header allowHeaders;
        allowHeaders.name = "Access-Control-Allow-Headers";
        allowHeaders.value = "Content-Type";

        HTTPServer::response_header allowMethods;
        allowMethods.name = "Access-Control-Allow-Methods";
        allowMethods.value = "GET,PUT,OPTIONS";

        HTTPServer::response_header allowOrigin;
        allowOrigin.name = "Access-Control-Allow-Origin";
        allowOrigin.value = "*";

        response.headers.push_back( allowHeaders );
        response.headers.push_back( allowMethods );
        response.headers.push_back( allowOrigin );
    }

    void* _socket;
};

}
}

#endif
