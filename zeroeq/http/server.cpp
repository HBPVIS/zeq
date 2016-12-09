
/* Copyright (c) 2016, Human Brain Project
 *                     Stefan.Eilemann@epfl.ch
 *                     Daniel.Nachbaur@epfl.ch
 */

#include "server.h"

#include "requestHandler.h"

#include "../log.h"
#include "../detail/broker.h"
#include "../detail/sender.h"
#include "../detail/socket.h"

#include "jsoncpp/json/json.h"

#include <servus/serializable.h>

#include <algorithm>
#include <thread>

namespace
{
// Transform camelCase to hyphenated-notation, e.g.
// lexis/render/LookOut -> lexis/render/look-out
// Inspiration: https://gist.github.com/rodamber/2558e25d4d8f6b9f2ffdf7bd49471340
std::string _camelCaseToHyphenated( std::string camelCase )
{
    std::string str( 1, tolower(camelCase[0]) );
    for( auto it = camelCase.begin() + 1; it != camelCase.end(); ++it )
    {
        if( isupper(*it) && *(it-1) != '-' && islower(*(it-1)))
            str += "-";
        str += *it;
    }

    std::transform( str.begin(), str.end(), str.begin(), ::tolower );
    return str;
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
void _convertEndpointName( std::string& endpoint )
{
    endpoint = _camelCaseToHyphenated( _replaceAll( endpoint, "::", "/" ));
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

} // unnamed namespace


namespace zeroeq
{
namespace http
{

class Server::Impl : public detail::Sender
{
public:
    Impl() : Impl( URI( )) {}

    Impl( const URI& uri_ )
        : detail::Sender( URI( _getInprocURI( )), 0, ZMQ_PAIR )
        , _requestHandler( _getInprocURI(), getContext( ))
        , _httpOptions( _requestHandler )
        , _httpServer( _httpOptions.
                       // INADDR_ANY translation: zmq -> boost.asio
                       address( uri_.getHost() == "*" ? "0.0.0.0"
                                                      : uri_.getHost( )).
                       port( std::to_string( int(uri_.getPort( )))).
                       reuse_address( true ) )
    {
        if( ::zmq_bind( socket, _getInprocURI().c_str( )) == -1 )
        {
            ZEROEQTHROW( std::runtime_error(
                             "Cannot bind HTTPServer to inproc socket" ));
        }

        try
        {
            _httpServer.listen();
            _httpThread.reset( new std::thread(
                               std::bind( &HTTPServer::run, &_httpServer )));
        }
        catch( const std::exception& e )
        {
            ZEROEQTHROW( std::runtime_error(
                             std::string( "Error while starting HTTP server: " )
                             + e.what( )));
        }

        uri = URI();
        uri.setHost( _httpServer.address( ));
        uri.setPort( std::stoi( _httpServer.port( )));
    }

    ~Impl()
    {
        if( _httpThread )
        {
            _httpServer.stop();
            _httpThread->join();
        }
    }

    bool remove( const servus::Serializable& serializable )
    {
        return remove( serializable.getTypeName( ));
    }

    bool remove( std::string endpoint )
    {
        _convertEndpointName( endpoint );
        _schemas.erase( endpoint );
        const bool foundPUT = _put.erase( endpoint ) != 0;
        const bool foundGET = _get.erase( endpoint ) != 0;
        return foundPUT || foundGET;
    }

    bool handlePUT( const std::string& endpoint,
                    servus::Serializable& serializable )
    {
        const auto func = [&serializable]( const std::string& json )
            { return serializable.fromJSON( json ); };
        return handlePUT( endpoint, serializable.getSchema(), func );
    }

    bool handlePUT( std::string endpoint, const std::string& schema,
                    const PUTPayloadFunc& func )
    {
        _convertEndpointName( endpoint );
        if( endpoint == REQUEST_REGISTRY )
            ZEROEQTHROW( std::runtime_error(
                             "'registry' not allowed as endpoint name" ));;

        if( _put.count( endpoint ) != 0 )
            return false;

        _put[ endpoint ] = func;
        {
            const std::string& exist = _returnSchema( endpoint );
            if( exist.empty( ))
                _schemas[ endpoint ] = schema;
            else if( schema != exist )
                ZEROEQTHROW( std::runtime_error(
                       "Schema registered for endpoint differs: " + endpoint ));
        }
        return true;
    }

    bool handleGET( const std::string& endpoint,
                    const servus::Serializable& serializable )
    {
        const auto func = [&serializable] { return serializable.toJSON(); };
        return handleGET( endpoint, serializable.getSchema(), func );
    }

    bool handleGET( std::string endpoint, const std::string& schema,
                    const GETFunc& func )
    {

        _convertEndpointName( endpoint );
        if( endpoint == REQUEST_REGISTRY )
            ZEROEQTHROW( std::runtime_error(
                             "'registry' not allowed as endpoint name" ));

        if( _get.count( endpoint ) != 0 )
            return false;

        _get[ endpoint ] = func;
        if( !schema.empty( ))
        {
            const std::string& exist = _returnSchema( endpoint );
            if( exist.empty( ))
                _schemas[ endpoint ] = schema;
            else if( schema != exist )
                ZEROEQTHROW( std::runtime_error(
                       "Schema registered for endpoint differs: " + endpoint ));

        }
        return true;
    }

    std::string getSchema( std::string endpoint ) const
    {
        _convertEndpointName( endpoint );
        return _returnSchema( endpoint );
    }

    void addSockets( std::vector< detail::Socket >& entries )
    {
        detail::Socket entry;
        entry.socket = socket;
        entry.events = ZMQ_POLLIN;
        entries.push_back( entry );
    }

    void process()
    {
        HTTPRequest* request = nullptr;
        ::zmq_recv( socket, &request, sizeof( request ), 0 );
        if( !request )
            ZEROEQTHROW( std::runtime_error(
                           "Could not receive HTTP request from HTTP server" ));

        switch( request->method )
        {
        case HTTPRequest::Method::GET:
            _processGET( *request );
            break;
        case HTTPRequest::Method::PUT:
            _processPUT( *request );
            break;
        default:
            ZEROEQTHROW( std::runtime_error(
                            "Encountered invalid HTTP method to process: " +
                             std::to_string( int( request->method ))));
        }

        bool done = true;
        ::zmq_send( socket, &done, sizeof( done ), 0 );
    }

protected:
    // key stores endpoints lower-case, hyphenated with '/' separators
    typedef std::map< std::string, PUTPayloadFunc > PUTFuncMap;
    typedef std::map< std::string, GETFunc > GETFuncMap;
    typedef std::map< std::string, std::string > SchemaMap;
    PUTFuncMap _put;
    GETFuncMap _get;
    SchemaMap _schemas;

    RequestHandler _requestHandler;
    HTTPServer::options _httpOptions;
    HTTPServer _httpServer;
    std::unique_ptr< std::thread > _httpThread;

    std::string _getInprocURI() const
    {
        std::ostringstream inprocURI;
        inprocURI << "inproc://#" << static_cast< const void* >( this );
        return inprocURI.str();
    }

    std::string _getEndpoint( const std::string& url )
    {
        if( url.empty( ))
            return url;

        return _camelCaseToHyphenated( url.substr( 1 ));
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

    std::string _returnSchema( const std::string& endpoint ) const
    {
        const auto& i = _schemas.find( endpoint );
        return i != _schemas.end() ? i->second : std::string();
    }

    void _processGET( HTTPRequest& request )
    {
        request.status = HTTPServer::response::ok; // be optimistic

        const std::string& endpoint = _getEndpoint( request.url );
        const auto& i = _get.find( endpoint );
        if( i != _get.end( ))
        {
            request.reply = i->second();
            return;
        }

        if( endpoint == REQUEST_REGISTRY )
        {
            request.reply = _returnRegistry();
            return;
        }

        if( _endsWithSchema( endpoint ))
        {
            const auto& schema = _returnSchema( endpoint.substr( 0,
                                                 endpoint.find_last_of( '/' )));
            if( !schema.empty( ))
            {
                request.reply = schema;
                return;
            }
        }

        request.status = HTTPServer::response::not_found;
    }

    void _processPUT( HTTPRequest& request )
    {
        const std::string& endpoint = _getEndpoint( request.url );
        const auto& i = _put.find( endpoint );

        if( i == _put.end( ))
            request.status = HTTPServer::response::not_found;
        else
        {
            if( i->second( request.request ))
                request.status = HTTPServer::response::ok;
            else
                request.status = HTTPServer::response::bad_request;
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

bool Server::handle( const std::string& endpoint, servus::Serializable& object )
{
    return handlePUT( endpoint, object ) && handleGET( endpoint, object );
}

bool Server::remove( const servus::Serializable& object )
{
    return _impl->remove( object );
}

bool Server::remove( const std::string& endpoint )
{
    return _impl->remove( endpoint );
}

bool Server::handlePUT( servus::Serializable& object )
{
    return _impl->handlePUT( object.getTypeName(), object );
}

bool Server::handlePUT( const std::string& endpoint,
                        servus::Serializable& object )
{
    return _impl->handlePUT( endpoint, object );
}

bool Server::handlePUT( const std::string& endpoint, const PUTFunc& func )
{
    return _impl->handlePUT( endpoint, "",
                             [func]( const std::string& ) { return func(); } );
}

bool Server::handlePUT( const std::string& endpoint, const std::string& schema,
                        const PUTFunc& func )
{
    return _impl->handlePUT( endpoint, schema,
                             [func]( const std::string& ) { return func(); } );
}

bool Server::handlePUT( const std::string& endpoint,
                        const PUTPayloadFunc& func )
{
    return _impl->handlePUT( endpoint, "", func );
}

bool Server::handlePUT( const std::string& endpoint,const std::string& schema,
                        const PUTPayloadFunc& func )
{
    return _impl->handlePUT( endpoint, schema, func );
}

bool Server::handleGET( const servus::Serializable& object )
{
    return _impl->handleGET( object.getTypeName(), object );
}

bool Server::handleGET( const std::string& endpoint,
                        const servus::Serializable& object )
{
    return _impl->handleGET( endpoint, object );
}

bool Server::handleGET( const std::string& endpoint, const GETFunc& func )
{
    return _impl->handleGET( endpoint, "", func );
}

bool Server::handleGET( const std::string& endpoint, const std::string& schema,
                        const GETFunc& func )
{
    return _impl->handleGET( endpoint, schema, func );
}

std::string Server::getSchema( const servus::Serializable& object ) const
{
    return _impl->getSchema( object.getTypeName( ));
}

std::string Server::getSchema( const std::string& endpoint ) const
{
    return _impl->getSchema( endpoint );
}

void Server::addSockets( std::vector< detail::Socket >& entries )
{
    _impl->addSockets( entries );
}

void Server::process( detail::Socket&, const uint32_t )
{
    _impl->process();
}

}
}
