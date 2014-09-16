
/* Copyright (c) 2014, Human Brain Project
 *                     Daniel Nachbaur <daniel.nachbaur@epfl.ch>
 *                     Stefan.Eilemann@epfl.ch
 */

#include <zeq/zeq.h>
#include <lunchbox/rng.h>
#include <lunchbox/uri.h>

#define BOOST_TEST_MODULE zeq
#include <boost/test/unit_test.hpp>

namespace test
{
lunchbox::URI buildURI( const std::string& hostname = "" )
{
    lunchbox::RNG rng;
    const unsigned short port = (rng.get<uint16_t>() % 60000) + 1024;
    std::ostringstream uriStr;
    uriStr << "foo://" << (hostname.empty() ? "localhost" : hostname);
    uriStr << ":" << port;
    return lunchbox::URI( uriStr.str( ));
}

const std::vector< float > camera( 16, 42 );

void onCameraEvent( const zeq::Event& event )
{
    BOOST_CHECK( event.getType() == zeq::vocabulary::EVENT_CAMERA );
    const std::vector< float >& deserialized =
            zeq::vocabulary::deserializeCamera( event );
    BOOST_CHECK_EQUAL_COLLECTIONS( camera.begin(), camera.end(),
                                   deserialized.begin(), deserialized.end( ));
}

void onExitEvent( const zeq::Event& event )
{
    BOOST_CHECK_EQUAL( event.getType(), zeq::vocabulary::EVENT_EXIT );
    BOOST_CHECK_EQUAL( event.getSize(), 0 );
}
}
