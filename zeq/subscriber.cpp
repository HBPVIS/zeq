
/* Copyright (c) 2014, Human Brain Project
 *                     Daniel Nachbaur <daniel.nachbaur@epfl.ch>
 *                     Stefan.Eilemann@epfl.ch
 */

#include "subscriber.h"
#include "event.h"
#include "detail/broker.h"

#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <lunchbox/log.h>
#include <lunchbox/servus.h>
#include <zmq.h>
#include <map>

// for NI_MAXHOST
#ifdef _WIN32
#  include <Ws2tcpip.h>
#else
#  include <netdb.h>
#endif

namespace zeq
{
namespace detail
{

class Subscriber
{
public:
    Subscriber( const lunchbox::URI& uri )
        : _context( zmq_ctx_new( ))
        , _service( std::string( "_" ) + uri.getScheme() + "._tcp" )
    {
        if( uri.getScheme().empty( ))
            LBTHROW( std::runtime_error(
                         boost::lexical_cast< std::string >( uri ) +
                         " is not a valid URI (scheme is missing)."));

        if( !uri.getHost().empty() && uri.getPort() != 0 )
            _addConnection( buildZmqURI( uri ));
        else
        {
            const lunchbox::Strings& zmqURIs = _buildSubscriberURI();
            BOOST_FOREACH( const std::string& zmqURI, zmqURIs )
                _addConnection( zmqURI );
        }
        _service.beginBrowsing( lunchbox::Servus::IF_ALL );
    }

    ~Subscriber()
    {
        _service.endBrowsing();
        BOOST_FOREACH( SocketType socket, _subscribers )
        {
            if ( socket.second )
                zmq_close( socket.second );
        }
        zmq_ctx_destroy( _context );
    }

    bool receive( const uint32_t timeout )
    {
        std::vector< zmq_pollitem_t > entries;
        _refreshConnections( entries );

        const int iPoll = zmq_poll( entries.data(), entries.size(),
                                    timeout == LB_TIMEOUT_INDEFINITE? -1
                                                                    : timeout );
        if( iPoll == -1 )
        {
            LBWARN << "Cannot poll, got " << zmq_strerror( zmq_errno( ))
                   << std::endl;
            return false;
        }

        if( iPoll == 0 )
            /* No events signaled during poll */
            return false;

        BOOST_FOREACH( const zmq_pollitem_t& entry, entries )
        {
            if( !( entry.revents & ZMQ_POLLIN ))
                continue;

            zmq_msg_t msg;
            zmq_msg_init( &msg );
            zmq_msg_recv( &msg, entry.socket, 0 );

            uint64_t type;
            memcpy( &type, zmq_msg_data( &msg ), sizeof(type) );

            const bool more = zmq_msg_more( &msg );
            zmq_msg_close( &msg );
            if( !more )
                return false;

            zmq_msg_init( &msg );
            zmq_msg_recv( &msg, entry.socket, 0 );
            zeq::Event event( type );
            event.setData( zmq_msg_data( &msg ), zmq_msg_size( &msg ));
            zmq_msg_close( &msg );

            if( _eventFuncs.count( type ) != 0 )
                _eventFuncs[type]( event );
        }

        return true;
    }

    bool registerHandler( const uint64_t event, const EventFunc& func )
    {
        if( _eventFuncs.count( event ) != 0 )
            return false;
        _eventFuncs[event] = func;
        return true;
    }

    bool deregisterHandler( const uint64_t event )
    {
        return _eventFuncs.erase( event ) > 0;
    }

private:
    void _refreshConnections( std::vector< zmq_pollitem_t >& entries )
    {
        _service.browse( 0 );
        const lunchbox::Strings& instances = _service.getInstances();
        entries.resize( instances.size( ));

        size_t i = 0;
        BOOST_FOREACH( const std::string& instance, instances )
        {
            const std::string& zmqURI = _getZmqURI( instance );

            // New subscription
            if( _subscribers.count( zmqURI ) == 0 )
                _addConnection( zmqURI );

            zmq_pollitem_t &entry = entries[i];
            entry.socket = _subscribers[zmqURI];
            entry.events = ZMQ_POLLIN;
            ++i;
        }
    }

    void _addConnection( const std::string& zmqURI )
    {
        _subscribers[zmqURI] = zmq_socket( _context, ZMQ_SUB );

        if( zmq_connect( _subscribers[zmqURI], zmqURI.c_str( )) == -1 )
        {
            zmq_close( _subscribers[zmqURI] );
            _subscribers.erase( zmqURI );
            LBTHROW( std::runtime_error(
                         "Cannot connect subscriber to " + zmqURI + ", got " +
                         zmq_strerror( zmq_errno( ))));
        }

        if( zmq_setsockopt( _subscribers[zmqURI], ZMQ_SUBSCRIBE, "", 0 ) == -1 )
        {
            zmq_close( _subscribers[zmqURI] );
            _subscribers.erase( zmqURI );
            LBTHROW( std::runtime_error(
                         std::string( "Cannot set subscriber, got " ) +
                         zmq_strerror( zmq_errno( ))));
        }
    }

    lunchbox::Strings _buildSubscriberURI()
    {
        lunchbox::Strings uriList;

        const lunchbox::Strings& instances =
                _service.discover( lunchbox::Servus::IF_ALL, 500 );
        BOOST_FOREACH( const std::string& instance, instances )
        {
            uriList.push_back( _getZmqURI( instance ));
        }
        return uriList;
    }

    std::string _getZmqURI( const std::string& instance )
    {
        const size_t pos = instance.find( ":" );
        const std::string& host = instance.substr( 0, pos );
        const std::string& port = instance.substr( pos + 1 );

        return buildZmqURI( host, boost::lexical_cast< uint16_t >( port ));
    }

    typedef std::map< uint64_t, EventFunc > EventFuncs;
    typedef std::pair< std::string, void* > SocketType;
    typedef std::map< std::string, void* > SocketMap;

    void* _context;
    SocketMap _subscribers;
    EventFuncs _eventFuncs;
    lunchbox::Servus _service;
};
}

Subscriber::Subscriber( const lunchbox::URI& uri )
    : _impl( new detail::Subscriber( uri ))
{
}

Subscriber::~Subscriber()
{
    delete _impl;
}

bool Subscriber::receive( const uint32_t timeout )
{
    return _impl->receive( timeout );
}

bool Subscriber::registerHandler( const uint64_t event, const EventFunc& func )
{
    return _impl->registerHandler( event, func );
}

bool Subscriber::deregisterHandler( const uint64_t event )
{
    return _impl->deregisterHandler( event );
}

}
