
/* Copyright (c) 2014, Human Brain Project
 *                     Daniel Nachbaur <daniel.nachbaur@epfl.ch>
 */

#include "broker.h"

#include <boost/bind.hpp>

BOOST_AUTO_TEST_CASE(test_subscribe_to_same_schema)
{
    zeq::Publisher publisher( lunchbox::URI( "foo://" ));
    BOOST_CHECK_NO_THROW(
                zeq::Subscriber subscriber( lunchbox::URI( "foo://" )));
}

BOOST_AUTO_TEST_CASE(test_subscribe_to_different_schema)
{
    zeq::Publisher publisher( lunchbox::URI( "foo://" ));
    BOOST_CHECK_NO_THROW(
                zeq::Subscriber subscriber( lunchbox::URI( "bar://" )));
}

BOOST_AUTO_TEST_CASE(test_publish_receive)
{
    lunchbox::RNG rng;
    const unsigned short port = (rng.get<uint16_t>() % 60000) + 1024;
    const std::string& portStr = boost::lexical_cast< std::string >( port );
    zeq::Subscriber subscriber( lunchbox::URI( "foo://localhost:" + portStr ));
    BOOST_CHECK( subscriber.registerHandler( zeq::vocabulary::EVENT_CAMERA,
                                            boost::bind( &onCameraEvent, _1 )));

    zeq::Publisher publisher( lunchbox::URI( "foo://*:" + portStr ));

    bool received = false;
    for( size_t i = 0; i < 10; ++i )
    {
        BOOST_CHECK( publisher.publish(
                         zeq::vocabulary::serializeCamera( test::camera )));

        if( subscriber.receive( 100 ))
        {
            received = true;
            break;
        }
    }
    BOOST_CHECK( received );
}

BOOST_AUTO_TEST_CASE(test_publish_receive_zeroconf)
{
    zeq::Publisher publisher( lunchbox::URI( "foo://" ));
    zeq::Subscriber subscriber( lunchbox::URI( "foo://" ));

    BOOST_CHECK( subscriber.registerHandler( zeq::vocabulary::EVENT_CAMERA,
                                            boost::bind( &onCameraEvent, _1 )));
    bool received = false;
    for( size_t i = 0; i < 20; ++i )
    {
        BOOST_CHECK( publisher.publish(
                         zeq::vocabulary::serializeCamera( test::camera )));

        if( subscriber.receive( 100 ))
        {
            received = true;
            break;
        }
    }
    BOOST_CHECK( received );
}

BOOST_AUTO_TEST_CASE(test_publish_receive_late_zeroconf)
{
    zeq::Subscriber subscriber( lunchbox::URI( "foo://" ));
    zeq::Publisher publisher( lunchbox::URI( "foo://" ));

    BOOST_CHECK( subscriber.registerHandler( zeq::vocabulary::EVENT_CAMERA,
                                             boost::bind( &test::onCameraEvent, _1 )));
    bool received = false;
    for( size_t i = 0; i < 20; ++i )
    {
        BOOST_CHECK( publisher.publish(
                         zeq::vocabulary::serializeCamera( camera )));

        if( subscriber.receive( 100 ))
        {
            received = true;
            break;
        }
    }
    BOOST_CHECK( received );
}

BOOST_AUTO_TEST_CASE(test_publish_receive_empty_event)
{
    zeq::Publisher publisher( lunchbox::URI( "foo://" ));
    zeq::Subscriber subscriber( lunchbox::URI( "foo://" ));

    BOOST_CHECK( subscriber.registerHandler( zeq::vocabulary::EVENT_EXIT,
                                        boost::bind( &test::onExitEvent, _1 )));
    bool received = false;
    const zeq::Event event( zeq::vocabulary::EVENT_EXIT );
    for( size_t i = 0; i < 20; ++i )
    {
        BOOST_CHECK( publisher.publish( event ));

        if( subscriber.receive( 100 ))
        {
            received = true;
            break;
        }
    }
    BOOST_CHECK( received );
}
