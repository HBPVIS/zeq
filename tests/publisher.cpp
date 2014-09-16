
/* Copyright (c) 2014, Human Brain Project
 *                     Daniel Nachbaur <daniel.nachbaur@epfl.ch>
 *                     Stefan.Eilemann@epfl.ch
 */

#include "broker.h"

#include <lunchbox/servus.h>

BOOST_AUTO_TEST_CASE(test_create_uri_publisher)
{
    zeq::Publisher publisher( lunchbox::URI( "foo://" ));
}

BOOST_AUTO_TEST_CASE(test_create_invalid_uri_publisher)
{
    // invalid URI, no hostname allowed
    BOOST_CHECK_THROW(
        zeq::Publisher publisher( lunchbox::URI( "foo://localhost" )),
        std::runtime_error );
}

BOOST_AUTO_TEST_CASE(test_publish)
{
    zeq::Publisher publisher( lunchbox::URI( test::buildURI( "*" )));
    BOOST_CHECK( publisher.publish(
                     zeq::vocabulary::serializeCamera( test::camera )));
}

BOOST_AUTO_TEST_CASE(test_publish_empty_event)
{
    zeq::Publisher publisher( lunchbox::URI( test::buildURI( "*" )));
    BOOST_CHECK( publisher.publish( zeq::Event( zeq::vocabulary::EVENT_EXIT )));
}

BOOST_AUTO_TEST_CASE(test_multiple_publisher_on_same_host)
{
    zeq::Publisher publisher1( lunchbox::URI( test::buildURI( "*" )));
    zeq::Publisher publisher2( lunchbox::URI( test::buildURI( "*" )));
    zeq::Publisher publisher3( lunchbox::URI( test::buildURI( "*" )));

    lunchbox::Servus service( "_foo._tcp" );
    const lunchbox::Strings& instances =
            service.discover( lunchbox::Servus::IF_ALL, 1000 );
    BOOST_CHECK_EQUAL( instances.size(), 3 );
}
