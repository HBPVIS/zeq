
/* Copyright (c) 2016, Human Brain Project
 *                     Stefan.Eilemann@epfl.ch
 *                     Daniel.Nachbaur@epfl.ch
 */

#include "server.h"

#include "../log.h"
#include "../detail/broker.h"
#include "../detail/sender.h"
#include "../detail/socket.h"

#include "jsoncpp/json/json.h"

#include <servus/serializable.h>

#include <httpxx/BufferedMessage.hpp>
#include <httpxx/Error.hpp>
#include <httpxx/Message.hpp>
#include <httpxx/ResponseBuilder.hpp>

#include <algorithm>

namespace httpxx = ::http; // avoid confusion between httpxx and zeroeq::http

namespace
{
std::string _toLower( std::string value )
{
    std::transform( value.begin(), value.end(), value.begin(), ::tolower );
    return value;
}

// http://stackoverflow.com/questions/5343190
std::string _replaceAll( std::string subject, const std::string& search,
                         const std::string& replace )
{
    size_t pos = 0;
    while( (pos = subject.find( search, pos )) != std::string::npos )
    {
         subject.replace( pos, search.length(), replace );
         pos += replace.length();
    }
    return subject;
}

// convert name to lowercase with '/' separators instead of '::'
void _convertEventName( std::string& event )
{
    event = _toLower( _replaceAll( event, "::", "/" ));
}

const std::string REQUEST_REGISTRY = "registry";
const std::string REQUEST_SCHEMA = "schema";

bool _endsWithSchema( const std::string& uri )
{
    if( uri.length() < REQUEST_SCHEMA.length( ))
        return false;
    return uri.compare( uri.length() - REQUEST_SCHEMA.length(),
                        std::string::npos, REQUEST_SCHEMA ) == 0;
}

/**
  In a typical situation, user agents can discover via a preflight request
  whether a cross-origin resource is prepared to accept requests. The current
  implementation of this class does not support this mechanism and has to
  accept cross-origin resources. In order to achieve that, access control
  headers are added to all HTTP responses, meaning that all sources are accepted
  for all requests.
  More information can be found here: https://www.w3.org/TR/cors
*/
const std::string access_control_allow_origins = "*";
const std::string access_control_allow_headers = "Content-Type";
const std::string access_control_allow_methods = "GET,PUT,OPTIONS";

}

namespace zeroeq
{
namespace http
{

class Server::Impl : public detail::Sender
{
public:
    Impl() : Impl( URI( )) {}

    Impl( const URI& uri_ )
        : detail::Sender( uri_, 0, ZMQ_STREAM )
    {
        const std::string& zmqURI = buildZmqURI( uri );
        if( ::zmq_bind( socket, zmqURI.c_str( )) == -1 )
        {
            ZEROEQTHROW( std::runtime_error(
                             std::string( "Cannot bind http server socket '" ) +
                             zmqURI + "': " +
                             zmq_strerror( zmq_errno( )) +
                             ( zmq_errno() == ENODEV ?
                                   ": host name instead of device used?" : "" )
                             ));
        }
        initURI();
    }

    bool remove( const servus::Serializable& serializable )
    {
        return remove( serializable.getTypeName( ));
    }

    bool remove( std::string event )
    {
        _convertEventName( event );
        _schemas.erase( event );
        const bool foundPUT = _put.erase( event ) != 0;
        const bool foundGET = _get.erase( event ) != 0;
        return foundPUT || foundGET;
    }

    bool handlePUT( servus::Serializable& serializable )
    {
        const auto func = [&serializable]( const std::string& json )
            { return serializable.fromJSON( json ); };
        return handlePUT( serializable.getTypeName(), serializable.getSchema(),
                          func );
    }

    bool handlePUT( std::string event, const std::string& schema,
                    const PUTPayloadFunc& func )
    {
        _convertEventName( event );
        if( event == REQUEST_REGISTRY )
            ZEROEQTHROW( std::runtime_error(
                             "'registry' not allowed as event name" ));;

        if( _put.count( event ) != 0 )
            return false;

        _put[ event ] = func;
        {
            const std::string& exist = _returnSchema( event );
            if( exist.empty( ))
                _schemas[ event ] = schema;
            else if( schema != exist )
                ZEROEQTHROW( std::runtime_error(
                             "Schema registered for event differs: " + event ));
        }
        return true;
    }

    bool handleGET( servus::Serializable& serializable )
    {
        const auto func = [&serializable] { return serializable.toJSON(); };
        return handleGET( serializable.getTypeName(), serializable.getSchema(),
                          func );
    }

    bool handleGET( std::string event, const std::string& schema,
                    const GETFunc& func )
    {

        _convertEventName( event );
        if( event == REQUEST_REGISTRY )
            ZEROEQTHROW( std::runtime_error(
                             "'registry' not allowed as event name" ));

        if( _get.count( event ) != 0 )
            return false;

        _get[ event ] = func;
        if( !schema.empty( ))
        {
            const std::string& exist = _returnSchema( event );
            if( exist.empty( ))
                _schemas[ event ] = schema;
            else if( schema != exist )
                ZEROEQTHROW( std::runtime_error(
                             "Schema registered for event differs: " + event ));

        }
        return true;
    }

    std::string getSchema( std::string event ) const
    {
        _convertEventName( event );
        return _returnSchema( event );
    }

    void addSockets( std::vector< detail::Socket >& entries )
    {
        detail::Socket entry;
        entry.socket = socket;
        entry.events = ZMQ_POLLIN;
        entries.push_back( entry );
    }

    void process( const uint32_t timeout )
    {
        // Read http request
        httpxx::BufferedRequest request;
        uint8_t id[256];
        int idSize = 0;
        int flags = 0;
        while( !request.complete( ))
        {
            // id of client (used for reply)
            idSize = ::zmq_recv( socket, id, sizeof( id ), flags );
            if( idSize <= 0 )
            {
                if( flags == 0 || zmq_errno() != EAGAIN )
                    ZEROEQWARN << "HTTP server receive failed: "
                               << zmq_strerror( zmq_errno( )) << std::endl;
                return;
            }
            flags = 0;

            // msg body
            zmq_msg_t msg;
            zmq_msg_init( &msg );
            zmq_msg_recv( &msg, socket, 0 );
            const char* data = (const char*)zmq_msg_data( &msg );
            const size_t msgSize = zmq_msg_size( &msg );

            if( msgSize == 0 )
            {
                if( data )
                {
                    // Skip zero-sized "Peer-Address" or empty string messages
                    // interleaved in the communication with libzmq 4.1.4.
                    // Those may or may not be followed by a "real" message, so
                    // try receiving again and return if there is nothing (to
                    // avoid blocking the receiving thread).
                    flags = ZMQ_NOBLOCK;
                    zmq_msg_close( &msg );
                    continue;
                }

                if( zmq_errno() != EAGAIN )
                    ZEROEQWARN << "HTTP server receive failed: "
                               << zmq_strerror( zmq_errno( )) << std::endl;
                return;
            }

            size_t consumed = 0;
            try
            {
                while( !request.complete() && msgSize > consumed )
                    consumed += request.feed( data + consumed,
                                              msgSize - consumed );
            }
            catch( const httpxx::Error& )
            {
                zmq_msg_close( &msg );
                return; // garbage from client, ignore
            }
            if( msgSize > consumed )
                ZEROEQWARN << "Unconsumed request data: " << data + consumed
                           << std::endl;

            zmq_msg_close( &msg );
        }

        // Respond to request
        httpxx::ResponseBuilder response;
        std::string body;
        if( request.method() == httpxx::Method::get( ))
            body = _processGET( request, response );
        else if( request.method() == httpxx::Method::put( ))
        {
            if( request.has_header( "Content-Length" ))
                _processPUT( request, response );
            else
                response.set_status( 411 ); // Content-Length required
            body.clear(); // no response body
        }
        else
        {
            if( request.method() ==  httpxx::Method::options() )
                // OPTIONS requests are accepted since access control is not
                // currently implemented.
                response.set_status( 200 );
            else
                response.set_status( 405 ); // Method Not Allowed
            body.clear(); // no response body
        }


        // response header
        if( response.status() >= 400 && response.status() < 500 )
            body = response.to_string();
        response.headers()[ "Content-Length" ] =
            std::to_string( body.length( ));
        response.headers()[ "Access-Control-Allow-Origin" ] =
            access_control_allow_origins;
        response.headers()[ "Access-Control-Allow-Headers" ] =
            access_control_allow_headers;
        response.headers()[ "Access-Control-Allow-Methods" ] =
            access_control_allow_methods;
        const std::string& rep = response.to_string();

        if( !sendResponse( id, idSize, ZMQ_SNDMORE, timeout ))
            return;
        if( !sendResponse( rep.c_str(), rep.length(),
                      body.empty() ? 0 : ZMQ_SNDMORE, timeout ))
        {
            return;
        }

        // response body
        if( !body.empty( ))
        {
            if( !sendResponse( id, idSize, ZMQ_SNDMORE, timeout ))
                return;
            if( !sendResponse( body.c_str(), body.length(), 0, timeout ))
                return;
        }
    }

    bool sendResponse( const void* data, const size_t length, const int flags,
                       const uint32_t timeout )
    {
        while( ::zmq_send( socket, data, length,
                           flags | ZMQ_NOBLOCK ) != int( length ))
        {
            // could be disconnect, send buffer full, ...
            if( zmq_errno() == EAGAIN )
            {
                detail::Socket pollItem;
                pollItem.socket = socket;
                pollItem.events = ZMQ_POLLERR;
                if( ::zmq_poll( &pollItem, 1, timeout == TIMEOUT_INDEFINITE
                                              ? -1 : timeout ) > 0 )
                {
                    // client still alive, send again
                    continue;
                }
                else if( zmq_errno() != EAGAIN )
                    ZEROEQWARN << "HTTP server poll failed: "
                               << zmq_strerror( zmq_errno( )) << std::endl;
            }
            else
                ZEROEQWARN << "HTTP server send failed: "
                           << zmq_strerror( zmq_errno( )) << std::endl;
            return false;
        }
        return true;
    }

protected:
    // key stores event lower-case with '/' separators
    typedef std::map< std::string, PUTPayloadFunc > PUTFuncMap;
    typedef std::map< std::string, GETFunc > GETFuncMap;
    typedef std::map< std::string, std::string > SchemaMap;
    PUTFuncMap _put;
    GETFuncMap _get;
    SchemaMap _schemas;

    std::string _getTypeName( const std::string& url )
    {
        if( url.empty( ))
            return url;

        return _toLower( url.substr( 1 ));
    }

    std::string _returnRegistry() const
    {
        Json::Value body( Json::objectValue );
        for( const auto& i : _get )
            body[i.first].append( "GET" );
        for( const auto& i : _put )
            body[i.first].append( "PUT" );

        return body.toStyledString();
    }

    std::string _returnSchema( const std::string& type ) const
    {
        const auto& i = _schemas.find( type );
        return i != _schemas.end() ? i->second : std::string();
    }

    std::string _processGET( const httpxx::BufferedRequest& request,
                             httpxx::ResponseBuilder& response )
    {
        response.set_status( 200 ); // be optimistic

        const std::string& type = _getTypeName( request.url( ));
        const auto& i = _get.find( type );
        if( i != _get.end( ))
            return i->second();

        if( type == REQUEST_REGISTRY )
            return _returnRegistry();

        if( _endsWithSchema( type ))
        {
            const auto& schema = _returnSchema( type.substr( 0,
                                                     type.find_last_of( '/' )));
            if( !schema.empty( ))
                return schema;
        }

        response.set_status( 404 );
        return std::string();
    }

    void _processPUT( const httpxx::BufferedRequest& request,
                      httpxx::ResponseBuilder& response )
    {
        const std::string& type = _getTypeName( request.url( ));
        const auto& i = _put.find( type );

        if( i == _put.end( ))
            response.set_status( 404 );
        else
        {
            if( i->second( request.body( )))
                response.set_status( 200 );
            else
                response.set_status( 400 );
        }
    }

};

namespace
{
std::string _getServerParameter( const int argc, const char* const* argv )
{
    for( int i = 0; i < argc; ++i  )
    {
        if( std::string( argv[i] ) == "--zeroeq-http-server" )
        {
            if( i == argc - 1 || argv[ i + 1 ][0] == '-' )
                return "tcp://";
            return argv[i+1];
        }
    }
    return std::string();
}
}

Server::Server( const URI& uri, Receiver& shared )
    : Receiver( shared )
    , _impl( new Impl( uri ))
{}

Server::Server( const URI& uri )
    : Receiver()
    , _impl( new Impl( uri ))
{}

Server::Server( Receiver& shared )
    : Receiver( shared )
    , _impl( new Impl )
{}

Server::Server()
    : Receiver()
    , _impl( new Impl )
{}

Server::~Server()
{}

std::unique_ptr< Server > Server::parse( const int argc,
                                         const char* const* argv )
{
    const std::string& param = _getServerParameter( argc, argv );
    if( param.empty( ))
        return nullptr;

    return std::unique_ptr< Server >( new Server( URI( param )));
}

std::unique_ptr< Server > Server::parse( const int argc,
                                         const char* const* argv,
                                         Receiver& shared )
{
    const std::string& param = _getServerParameter( argc, argv );
    if( param.empty( ))
        return nullptr;

    return std::unique_ptr< Server >( new Server( URI( param ), shared ));
}

const URI& Server::getURI() const
{
    return _impl->uri;
}

SocketDescriptor Server::getSocketDescriptor() const
{
    SocketDescriptor fd = 0;
    size_t fdLength = sizeof(fd);
    if( ::zmq_getsockopt( _impl->socket, ZMQ_FD, &fd, &fdLength ) == -1 )
    {
        ZEROEQTHROW( std::runtime_error(
                         std::string( "Could not get socket descriptor'" )));
    }
    return fd;
}

bool Server::remove( const servus::Serializable& object )
{
    return _impl->remove( object );
}

bool Server::remove( const std::string& event )
{
    return _impl->remove( event );
}

bool Server::handlePUT( servus::Serializable& object )
{
    return _impl->handlePUT( object );
}

bool Server::handlePUT( const std::string& event, const PUTFunc& func )
{
    return _impl->handlePUT( event, "",
                             [func]( const std::string& ) { return func(); } );
}

bool Server::handlePUT( const std::string& event, const std::string& schema,
                        const PUTFunc& func )
{
    return _impl->handlePUT( event, schema,
                             [func]( const std::string& ) { return func(); } );
}

bool Server::handlePUT( const std::string& event, const PUTPayloadFunc& func )
{
    return _impl->handlePUT( event, "", func );
}

bool Server::handlePUT( const std::string& event,const std::string& schema,
                        const PUTPayloadFunc& func )
{
    return _impl->handlePUT( event, schema, func );
}

bool Server::handleGET( servus::Serializable& object )
{
    return _impl->handleGET( object );
}

bool Server::handleGET( const std::string& event, const GETFunc& func )
{
    return _impl->handleGET( event, "", func );
}

bool Server::handleGET( const std::string& event, const std::string& schema,
                        const GETFunc& func )
{
    return _impl->handleGET( event, schema, func );
}

std::string Server::getSchema( const servus::Serializable& object ) const
{
    return _impl->getSchema( object.getTypeName( ));
}

std::string Server::getSchema( const std::string& event ) const
{
    return _impl->getSchema( event );
}

void Server::addSockets( std::vector< detail::Socket >& entries )
{
    _impl->addSockets( entries );
}

void Server::process( detail::Socket&, const uint32_t timeout )
{
    _impl->process( timeout );
}

}
}
