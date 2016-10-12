
/* Copyright (c) 2014-2015, Human Brain Project
 *                          Daniel Nachbaur <daniel.nachbaur@epfl.ch>
 *                          Stefan.Eilemann@epfl.ch
 */

#define BOOST_TEST_MODULE zeroeq_pub_sub

#include "broker.h"
#include <zeroeq/detail/sender.h>

#include <servus/servus.h>
#include <servus/uri.h>

#include <thread>
#include <chrono>

BOOST_AUTO_TEST_CASE(publish_receive_serializable)
{
    test::Echo echoOut( "The quick brown fox" );
    test::Echo echoIn;

    zeroeq::Publisher publisher( zeroeq::NULL_SESSION );
    zeroeq::Subscriber subscriber( zeroeq::URI( publisher.getURI( )));
    BOOST_CHECK( subscriber.subscribe( echoIn ));

    for( size_t i = 0; i < 10; ++i )
    {
        BOOST_CHECK( publisher.publish( echoOut ));

        if( subscriber.receive( 100 ))
        {
            BOOST_CHECK_EQUAL( echoIn.getMessage(), echoOut.getMessage( ));
            return;
        }
    }
    BOOST_CHECK( !"reachable" );
}

BOOST_AUTO_TEST_CASE(publish_receive_event)
{
    const std::string echoString( "The quick brown fox" );
    zeroeq::Publisher publisher( zeroeq::NULL_SESSION );
    zeroeq::Subscriber subscriber( zeroeq::URI( publisher.getURI( )));
    bool received = false;
    BOOST_CHECK( subscriber.subscribe( zeroeq::make_uint128( "Echo" ),
        zeroeq::EventPayloadFunc([&]( const void* data , const size_t size )
        {
            BOOST_CHECK_EQUAL( reinterpret_cast< const char* >( data ),
                               echoString );
            BOOST_CHECK_EQUAL( size, echoString.length( ));
            received = true;
        })));

    for( size_t i = 0; i < 10; ++i )
    {
        BOOST_CHECK( publisher.publish( zeroeq::make_uint128( "Echo" ),
                                        echoString.c_str(),
                                        echoString.length( )));

        if( subscriber.receive( 100 ))
        {
            BOOST_CHECK( received );
            return;
        }
    }
    BOOST_CHECK( !"reachable" );
}

BOOST_AUTO_TEST_CASE(publish_receive_empty_event)
{
    zeroeq::Publisher publisher( zeroeq::NULL_SESSION );
    zeroeq::Subscriber subscriber( zeroeq::URI( publisher.getURI( )));
    bool received = false;
    BOOST_CHECK( subscriber.subscribe( zeroeq::make_uint128( "Empty" ),
        zeroeq::EventFunc([&]() { received = true; })));

    for( size_t i = 0; i < 10; ++i )
    {
        BOOST_CHECK( publisher.publish( zeroeq::make_uint128( "Empty" )));

        if( subscriber.receive( 100 ))
        {
            BOOST_CHECK( received );
            return;
        }
    }
    BOOST_CHECK( !"reachable" );
}

BOOST_AUTO_TEST_CASE(no_receive)
{
    zeroeq::Subscriber subscriber( zeroeq::URI( "1.2.3.4:1234" ));
    BOOST_CHECK( !subscriber.receive( 100 ));
}

BOOST_AUTO_TEST_CASE( subscribe_to_same_session_zeroconf )
{
    if( !servus::Servus::isAvailable() || getenv("TRAVIS"))
        return;

    zeroeq::Publisher publisher( test::buildUniqueSession( ));
    BOOST_CHECK_NO_THROW(
        zeroeq::Subscriber subscriber( publisher.getSession( )));
}

BOOST_AUTO_TEST_CASE(subscribe_to_different_session_zeroconf)
{
    if( !servus::Servus::isAvailable() || getenv("TRAVIS"))
        return;

    zeroeq::Publisher publisher( test::buildUniqueSession( ));
    BOOST_CHECK_NO_THROW(
               zeroeq::Subscriber subscriber( publisher.getSession() + "bar" ));
}

BOOST_AUTO_TEST_CASE(no_receive_zeroconf)
{
    if( !servus::Servus::isAvailable() || getenv("TRAVIS"))
        return;

    zeroeq::Subscriber subscriber( test::buildUniqueSession( ));
    BOOST_CHECK( !subscriber.receive( 100 ));
}

BOOST_AUTO_TEST_CASE(publish_receive_zeroconf)
{
    if( !servus::Servus::isAvailable() || getenv("TRAVIS"))
        return;

    zeroeq::Publisher publisher( test::buildUniqueSession( ));
    zeroeq::Subscriber noSubscriber( publisher.getSession( ));
    zeroeq::detail::Sender::getUUID() = servus::make_UUID(); // different machine
    zeroeq::Subscriber subscriber( publisher.getSession( ));

    BOOST_CHECK( subscriber.subscribe( test::Echo::IDENTIFIER(),
                                       &test::onEchoEvent ));
    BOOST_CHECK( noSubscriber.subscribe( test::Echo::IDENTIFIER(),
                                         &test::onEchoEvent ));

    bool received = false;
    for( size_t i = 0; i < 20; ++i )
    {
        BOOST_CHECK( publisher.publish( test::Echo( test::echoMessage )));

        BOOST_CHECK( !noSubscriber.receive( 100 ));
        if( subscriber.receive( 100 ))
        {
            received = true;
            break;
        }
    }
    BOOST_CHECK( received );
}

BOOST_AUTO_TEST_CASE(publish_receive_zeroconf_disabled)
{
    if( getenv("TRAVIS"))
        return;

    zeroeq::Publisher publisher( zeroeq::NULL_SESSION );
    zeroeq::Subscriber subscriber( test::buildUniqueSession( ));

    BOOST_CHECK( subscriber.subscribe( test::Echo::IDENTIFIER(),
                                       &test::onEchoEvent ));

    bool received = false;
    for( size_t i = 0; i < 20; ++i )
    {
        BOOST_CHECK( publisher.publish( test::Echo( test::echoMessage )));

        if( subscriber.receive( 100 ))
        {
            received = true;
            break;
        }
    }
    BOOST_CHECK( !received );
}

BOOST_AUTO_TEST_CASE(publish_receive_filters)
{
    // The publisher needs to be destroyed before the subscriber otherwise
    // zmq_ctx_destroy() can hang forever. For more details see
    // zmq_ctx_destroy() documentation.
    zeroeq::Publisher* publisher = new zeroeq::Publisher( zeroeq::NULL_SESSION );
    zeroeq::Subscriber subscriber( zeroeq::URI( publisher->getURI( )));

    // Make sure we're connected
    BOOST_CHECK( subscriber.subscribe( test::Echo::IDENTIFIER(),
                                       &test::onEchoEvent ));
    for( size_t i = 0; i < 20; ++i )
    {
        BOOST_CHECK( publisher->publish( test::Echo( test::echoMessage )));
        if( subscriber.receive( 100 ))
            break;
    }
    BOOST_CHECK( subscriber.unsubscribe( test::Echo::IDENTIFIER( )));

    // benchmark with no data to be transmitted
    auto startTime = std::chrono::high_resolution_clock::now();
    for( size_t i = 0; i < 20000; ++i )
    {
        BOOST_CHECK( publisher->publish( test::Echo( )));
        while( subscriber.receive( 0 )) /* NOP to drain */;
    }
    const auto& noEchoTime = std::chrono::high_resolution_clock::now() -
                             startTime;

    // Benchmark with echo handler, now should send data
    const std::string message( 60000, 'a' );
    BOOST_CHECK( subscriber.subscribe( test::Echo::IDENTIFIER(),
                  zeroeq::EventPayloadFunc([&]( const void*, const size_t size )
                  { BOOST_CHECK_EQUAL( size, message.size( )); }) ));

    startTime = std::chrono::high_resolution_clock::now();
    for( size_t i = 0; i < 20000; ++i )
    {
        BOOST_CHECK( publisher->publish( test::Echo( message )));
        while( subscriber.receive( 0 )) /* NOP to drain */;
    }

    const auto& echoTime = std::chrono::high_resolution_clock::now() -
                           startTime;

    BOOST_CHECK_MESSAGE( noEchoTime < echoTime,
                         std::chrono::nanoseconds( noEchoTime ).count() << ", "
                         << std::chrono::nanoseconds( echoTime ).count( ));
    delete publisher;
}

BOOST_AUTO_TEST_CASE(publish_receive_late_zeroconf)
{
    if( !servus::Servus::isAvailable() || getenv("TRAVIS"))
        return;

    zeroeq::Subscriber subscriber( test::buildUniqueSession( ));
    zeroeq::detail::Sender::getUUID() = servus::make_UUID(); // different machine
    zeroeq::Publisher publisher( subscriber.getSession( ));

    BOOST_CHECK( subscriber.subscribe( test::Echo::IDENTIFIER(),
                                       &test::onEchoEvent ));
    bool received = false;
    for( size_t i = 0; i < 20; ++i )
    {
        BOOST_CHECK( publisher.publish( test::Echo( test::echoMessage )));

        if( subscriber.receive( 100 ))
        {
            received = true;
            break;
        }
    }
    BOOST_CHECK( received );
}

BOOST_AUTO_TEST_CASE(publish_receive_empty_event_zeroconf)
{
    if( !servus::Servus::isAvailable() || getenv("TRAVIS"))
        return;

    zeroeq::Publisher publisher( test::buildUniqueSession( ));
    zeroeq::detail::Sender::getUUID() = servus::make_UUID(); // different machine
    zeroeq::Subscriber subscriber( publisher.getSession( ));

    BOOST_CHECK( subscriber.subscribe( test::Empty::IDENTIFIER(),
                                       zeroeq::EventFunc([]{})));

    bool received = false;
    for( size_t i = 0; i < 20; ++i )
    {
        BOOST_CHECK( publisher.publish( test::Empty( )));

        if( subscriber.receive( 100 ))
        {
            received = true;
            break;
        }
    }
    BOOST_CHECK( received );
}

namespace
{
class Publisher
{
public:
    Publisher()
        : running( false )
    {}

    void run( const std::string& session )
    {
        zeroeq::Publisher publisher( session );
        running = true;
        size_t i = 0;
        while( running )
        {
            BOOST_CHECK( publisher.publish( test::Echo( test::echoMessage )));
            std::this_thread::sleep_for( std::chrono::milliseconds( 100 ));
            ++i;

            if( i > 300 )
                ZEROEQTHROW( std::runtime_error(
                                 "Publisher giving up after 30s" ));
        }
    }

    bool running;
};
}

BOOST_AUTO_TEST_CASE(publish_blocking_receive_zeroconf)
{
    if( !servus::Servus::isAvailable() || getenv("TRAVIS"))
        return;

    zeroeq::Subscriber subscriber( test::buildUniqueSession( ));
    zeroeq::detail::Sender::getUUID() = servus::make_UUID(); // different machine

    BOOST_CHECK( subscriber.subscribe( test::Echo::IDENTIFIER(),
                                       &test::onEchoEvent ));

    Publisher publisher;
    std::thread thread( std::bind( &Publisher::run, &publisher,
                                   subscriber.getSession( )));

    BOOST_CHECK( subscriber.receive( ));

    publisher.running = false;
    thread.join( );
}
