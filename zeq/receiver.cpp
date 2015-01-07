
/* Copyright (c) 2014, Human Brain Project
 *                     Daniel Nachbaur <daniel.nachbaur@epfl.ch>
 *                     Stefan.Eilemann@epfl.ch
 */

#define NOMINMAX // otherwise std::min/max below don't work on VS

#include "receiver.h"
#include "detail/socket.h"

#include <boost/foreach.hpp>
#include <lunchbox/clock.h>
#include <lunchbox/log.h>
#include <algorithm>
#include <deque>

namespace zeq
{
namespace detail
{
class Receiver
{
public:
    Receiver()
        : _context( zmq_ctx_new( ))
    {}

    ~Receiver()
    {
        zmq_ctx_destroy( _context );
    }

    void add( ::zeq::Receiver* receiver )
    {
        _shared.push_back( receiver );
    }

    void remove( ::zeq::Receiver* receiver )
    {
        _shared.erase( std::remove( _shared.begin(), _shared.end(), receiver ),
                       _shared.end( ));
    }

    bool receive( const uint32_t timeout )
    {
        if( timeout == LB_TIMEOUT_INDEFINITE )
            return _receive();

        // Never fully block. Give receivers a chance to update, e.g., to check
        // for new connections from zeroconf (#20)
        const uint32_t block = ( std::min )( 1000u, timeout / 10 );

        lunchbox::Clock timer;
        while( true )
        {
            BOOST_FOREACH( ::zeq::Receiver* receiver, _shared )
                receiver->update();

            const uint64_t elapsed = timer.getTime64();
            long wait = 0;
            if( elapsed < timeout )
                wait = ( std::min )( timeout - uint32_t( elapsed ), block );

            if( _receive( wait ))
                return true;

            if( elapsed >= timeout )
                return false;
        }
    }

    void* getZMQContext() { return _context; }

private:
    void* _context;
    typedef std::vector< ::zeq::Receiver* > Receivers;
    typedef Receivers::iterator ReceiversIter;

    Receivers _shared;

    bool _receive()
    {
        while( true )
        {
            BOOST_FOREACH( ::zeq::Receiver* receiver, _shared )
                receiver->update();

            // Never fully block. Give receivers a chance to update, e.g., to
            // check for new connections from zeroconf (#20)
            if( _receive( 1000 ))
                return true;
        }
    }

    bool _receive( const uint32_t timeout )
    {
        std::vector< Socket > sockets;
        std::deque< size_t > intervals;
        BOOST_FOREACH( ::zeq::Receiver* receiver, _shared )
        {
            const size_t before = sockets.size();
            receiver->addSockets( sockets );
            intervals.push_back( sockets.size() - before );
        }

        switch( zmq_poll( sockets.data(), int( sockets.size( )),
                          timeout == LB_TIMEOUT_INDEFINITE ? -1 : timeout ))
        {
        case -1: // error
            LBTHROW( std::runtime_error( std::string( "Poll error: " ) +
                                         zmq_strerror( zmq_errno( ))));
        case 0: // timeout; no events signaled during poll
            return false;

        default:
        {
            // For each event, find the subscriber which supplied the socket and
            // inform it in case there is data on the socket. We saved #sockets
            // for each subscriber above and track them down here as we iterate
            // over all sockets:
            ReceiversIter i = _shared.begin();
            size_t interval = intervals.front();
            intervals.pop_front();

            BOOST_FOREACH( Socket& socket, sockets )
            {
                while( interval == 0 || interval-- == 0 )
                {
                    ++i;
                    interval = intervals.front();
                    intervals.pop_front();
                }

                if( socket.revents & ZMQ_POLLIN )
                    (*i)->process( socket );
            }
            return true;
        }
        }
    }
};
}

Receiver::Receiver()
    : _impl( new detail::Receiver )
{
    _impl->add( this );
}

Receiver::Receiver( Receiver& shared )
    : boost::noncopyable()
    , _impl( shared._impl )
{
    _impl->add( this );
}

Receiver::~Receiver()
{
    _impl->remove( this );
}

bool Receiver::receive( const uint32_t timeout )
{
    return _impl->receive( timeout );
}

void* Receiver::getZMQContext()
{
    return _impl->getZMQContext();
}

}
