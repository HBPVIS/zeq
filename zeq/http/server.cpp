
/* Copyright (c) 2016, Human Brain Project
 *                     Stefan.Eilemann@epfl.ch
 */

#include <zeq/http/server.h>

#include "../log.h"
#include "../detail/broker.h"
#include "../detail/sender.h"
#include "../detail/socket.h"
#include <servus/serializable.h>
#include <httpxx/BufferedMessage.hpp>
#include <httpxx/Error.hpp>
#include <httpxx/Message.hpp>
#include <httpxx/ResponseBuilder.hpp>
#include <algorithm>

namespace httpxx = ::http; // avoid confusion between httpxx and zeq::http

namespace
{
std::string _toLower( std::string value )
{
    std::transform( value.begin(), value.end(), value.begin(), ::tolower );
    return value;
}

const std::string access_control_allow_origins = "*";
const std::string access_control_allow_headers = "Content-Type";
const std::string access_control_allow_methods = "GET,PUT,OPTIONS";

}

namespace zeq
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
            ZEQTHROW( std::runtime_error(
                      std::string( "Cannot bind http server socket '" ) +
                                   zmqURI + "': " +
                                   zmq_strerror( zmq_errno( )) +
                                   ( zmq_errno() == ENODEV ?
                                   ": host name instead of device used?" : "" )));
        }
        initURI();
    }

    bool subscribe( servus::Serializable& serializable )
    {
        const std::string& name = _toLower( serializable.getTypeName( ));
        if( _subscriptions.count( name ) != 0 )
            return false;

        _subscriptions[ name ] = &serializable;
        return true;
    }

    bool unsubscribe( const servus::Serializable& serializable )
    {
        return _subscriptions.erase(
            _toLower( serializable.getTypeName( ))) != 0;
    }

    bool register_( servus::Serializable& serializable )
    {
        const std::string& name = _toLower( serializable.getTypeName( ));
        if( _registrations.count( name ) != 0 )
            return false;

        _registrations[ name ] = &serializable;
        return true;
    }

    bool unregister( const servus::Serializable& serializable )
    {
        return _registrations.erase(
            _toLower( serializable.getTypeName( ))) != 0;
    }

    void addSockets( std::vector< detail::Socket >& entries )
    {
        detail::Socket entry;
        entry.socket = socket;
        entry.events = ZMQ_POLLIN;
        entries.push_back( entry );
    }

    void process( detail::Socket& )
    {
        // Read request and body
        httpxx::BufferedRequest request;
        std::string body;
        uint8_t id[256];
        int idSize = 0;
        while( !request.complete( ))
        {
            // id of client (used for reply)
            idSize = ::zmq_recv( socket, id, sizeof( id ), 0 );
            if( idSize <= 0 )
            {
                ZEQWARN << "HTTP server receive failed: "
                        << zmq_strerror( zmq_errno( )) << std::endl;
                return;
            }

            // msg body
            zmq_msg_t msg;
            zmq_msg_init( &msg );
            zmq_msg_recv( &msg, socket, 0 );
            const char* data = (const char*)zmq_msg_data( &msg );
            const size_t msgSize = zmq_msg_size( &msg );

            if( msgSize == 0 )
            {
                ZEQWARN << "HTTP server receive failed: "
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
            zmq_msg_close( &msg );
        }

        // Handle
        httpxx::ResponseBuilder response;
        if( request.method() == httpxx::Method::get( ))
            body = _processGet( request, response );
        else if( request.method() == httpxx::Method::put( ))
        {
            if( request.has_header( "Content-Length" ))
                _processPut( request, response );
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
        response.headers()[ "Content-Length" ] = std::to_string( body.length( ));
        response.headers()[ "Access-Control-Allow-Origin" ] =
            access_control_allow_origins;
        response.headers()[ "Access-Control-Allow-Headers" ] =
            access_control_allow_headers;
        response.headers()[ "Access-Control-Allow-Methods" ] =
            access_control_allow_methods;
        const std::string& rep = response.to_string();
        const int more = body.empty() ? 0 : ZMQ_SNDMORE;
        if( ::zmq_send( socket, id, idSize, ZMQ_SNDMORE ) != idSize ||
            ::zmq_send( socket, rep.c_str(), rep.length(), more ) !=
            int( rep.length( )))
        {
            ZEQWARN << "Could not send HTTP response header: "
                    << zmq_strerror( zmq_errno( )) << std::endl;
            return;
        }

        // response body
        if( !body.empty() &&
            ( ::zmq_send( socket, id, idSize, ZMQ_SNDMORE ) != idSize ||
              ::zmq_send( socket, body.c_str(), body.length(), 0 ) !=
              int( body.length( ))))
        {
            ZEQWARN << "Could not send HTTP response body: "
                    << zmq_strerror( zmq_errno( )) << std::endl;
        }
    }

protected:
    typedef std::map< std::string, servus::Serializable* > SerializableMap;
    SerializableMap _subscriptions;
    SerializableMap _registrations;

    std::string _getTypeName( const std::string& url )
    {
        if( url.empty( ))
            return url;

        std::string name = url.substr( 1 );
        while( true )
        {
            const size_t pos = name.find( '/' );
            if( pos == std::string::npos )
                return name;

            name = name.substr( 0, pos ) + "::" + name.substr( pos + 1 );
        }
    }

    std::string _processGet( const httpxx::BufferedRequest& request,
                             httpxx::ResponseBuilder& response )
    {
        const std::string& type = _toLower( _getTypeName( request.url( )));
        const auto& i = _registrations.find( type );

        if( i == _registrations.end( ))
        {
            response.set_status( 404 );
            return std::string();
        }

        response.set_status( 200 );
        i->second->notifyRequested();
        return i->second->toJSON();
    }

    void _processPut( const httpxx::BufferedRequest& request,
                      httpxx::ResponseBuilder& response )
    {
        const std::string& type = _toLower( _getTypeName( request.url( )));
        const auto& i = _subscriptions.find( type );

        if( i == _subscriptions.end( ))
            response.set_status( 404 );
        else
        {
            if( i->second->fromJSON( request.body( )))
            {
                i->second->notifyUpdated();
                response.set_status( 200 );
            }
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
        if( std::string( argv[i] ) == "--zeq-http-server" )
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


std::unique_ptr< Server > Server::parse( const int argc, const char* const* argv )
{
    const std::string& param = _getServerParameter( argc, argv );
    if( param.empty( ))
        return nullptr;

    return std::unique_ptr< Server >( new Server( URI( param )));
}

std::unique_ptr< Server > Server::parse( const int argc, const char* const* argv,
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
        ZEQTHROW( std::runtime_error(
                  std::string( "Could not get socket descriptor'" )));
    }
    return fd;
}

bool Server::subscribe( servus::Serializable& object )
{
    return _impl->subscribe( object );
}

bool Server::unsubscribe( const servus::Serializable& object )
{
    return _impl->unsubscribe( object );
}

bool Server::register_( servus::Serializable& object )
{
    return _impl->register_( object );
}

bool Server::unregister( const servus::Serializable& object )
{
    return _impl->unregister( object );
}

void Server::addSockets( std::vector< detail::Socket >& entries )
{
    _impl->addSockets( entries );
}

void Server::process( detail::Socket& socket )
{
    _impl->process( socket );
}

}
}
