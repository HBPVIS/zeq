
/* Copyright (c) 2016-2017, Human Brain Project
 *                          Stefan.Eilemann@epfl.ch
 *                          Daniel.Nachbaur@epfl.ch
 */

#include "requestHandler.h"

#include <zeroeq/log.h>
#include <zeroeq/uri.h>

#include <memory> // shared_from_this
#include <zmq.h>

namespace zeroeq
{
namespace http
{
namespace
{

// The actual handler for each incoming request where the data is read from
// a dedicated connection to the client.
struct ConnectionHandler : std::enable_shared_from_this< ConnectionHandler >
{
    ConnectionHandler( const HTTPServer::request& request, void* socket )
        : _request( request )
        , _socket( socket )
    {}

    void operator()( HTTPServer::connection_ptr connection )
    {
        if( _request.method == "PUT" )
        {
            for( const auto& i : _request.headers )
            {
                if( i.name == "Content-Length" )
                {
                    _size = std::stoi( i.value );
                    break;
                }
            }

            // if we have payload, schedule an (async) read of all chunks. Will
            // call _handleRequest() after all data has been read.
            if( _size > 0 )
            {
                _readChunk( connection );
                return;
            }
            _handleRequest( HTTPRequest::Method::PUT, connection );
        }
        else if( _request.method == "GET" )
            _handleRequest( HTTPRequest::Method::GET, connection );
        else
        {
            std::vector< HTTPServer::response_header > headers;
            _addCorsHeaders( headers );
            connection->set_status( HTTPServer::connection::not_supported );
            connection->set_headers( headers );
        }
    }

private:
    void _readChunk( HTTPServer::connection_ptr connection )
    {
        namespace pl = std::placeholders;
        connection->read( std::bind( &ConnectionHandler::_handleChunk,
                                     ConnectionHandler::shared_from_this(),
                                     pl::_1, pl::_2, pl::_3, connection ));
    }

    void _handleChunk( HTTPServer::connection::input_range range,
                       const boost::system::error_code error, const size_t size,
                       HTTPServer::connection_ptr connection )
    {
        if( error )
        {
            ZEROEQERROR << "Error during ConnectionHandler::_handleChunk: "
                        << error.message() << std::endl;
            return;
        }

        _body.append( std::begin( range ), size );
        _size -= size;
        if( _size > 0 )
            _readChunk( connection );
        else
            _handleRequest( HTTPRequest::Method::PUT, connection );
    }

    void _handleRequest( const HTTPRequest::Method& method,
                         HTTPServer::connection_ptr connection )
    {
        std::vector< HTTPServer::response_header > headers;
        _addCorsHeaders( headers );

        HTTPRequest httpRequest;
        httpRequest.method = method;
        httpRequest.request = _body;
        httpRequest.url = URI( _request.destination ).getPath();
        void* httpRequestPtr = &httpRequest;
        zmq_send( _socket, &httpRequestPtr, sizeof(void*), 0 );
        bool done;
        zmq_recv( _socket, &done, sizeof( done ), 0 );

        connection->set_status( httpRequest.status );

        const bool haveReply = !httpRequest.reply.empty();
        if( !haveReply )
        {
            connection->set_headers( headers );
            return;
        }

        HTTPServer::response_header contentLength;
        contentLength.name = "Content-Length";
        contentLength.value = std::to_string( httpRequest.reply.length( ));
        headers.push_back( contentLength );

        connection->set_headers( headers );
        connection->write( httpRequest.reply );
    }

    void _addCorsHeaders( std::vector< HTTPServer::response_header >& headers )
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

        headers.push_back( allowHeaders );
        headers.push_back( allowMethods );
        headers.push_back( allowOrigin );
    }

    const HTTPServer::request& _request;
    void* _socket;
    std::string _body;
    int _size = 0;
};
} // anonymous namespace

RequestHandler::RequestHandler( const std::string& zmqURL, void* zmqContext )
    : _socket( zmq_socket( zmqContext, ZMQ_PAIR ))
{
    if( zmq_connect( _socket, zmqURL.c_str( )) == -1 )
    {
        ZEROEQTHROW( std::runtime_error(
                        "Cannot connect RequestHandler to inproc socket" ));
    }
}

RequestHandler::~RequestHandler()
{
    zmq_close( _socket );
}

void RequestHandler::operator() ( const HTTPServer::request& request,
                                  HTTPServer::connection_ptr connection )
{
    // as the underlying cppnetlib http server is asynchronous and payload for
    // PUT events has to be read in chunks in the cppnetlib thread, create
    // a shared instance of the handler object that is passed to cppnetlib for
    // processing the request.
    std::shared_ptr< ConnectionHandler > connectionHandler
            (new ConnectionHandler( request, _socket ));
    (*connectionHandler)(connection);
}

}
}
