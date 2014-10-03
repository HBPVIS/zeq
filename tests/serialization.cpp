
/* Copyright (c) 2014, Human Brain Project
 *                     Daniel Nachbaur <daniel.nachbaur@epfl.ch>
 */

#include <zeq/zeq.h>
#include <lunchbox/uri.h>

#define BOOST_TEST_MODULE serialization
#include <boost/test/unit_test.hpp>
#include <boost/bind.hpp>

BOOST_AUTO_TEST_CASE(test_serialization)
{
    const std::vector< float > camera( 16, 42 );
    const zeq::Event& event = zeq::vocabulary::serializeCamera( camera );
    const std::vector< float >& deserialized =
            zeq::vocabulary::deserializeCamera( event );
    BOOST_CHECK_EQUAL_COLLECTIONS( camera.begin(), camera.end(),
                                   deserialized.begin(), deserialized.end( ));

    unsigned int ids[] = { 16, 2, 77, 29 };
    const std::vector< unsigned int > idList(
        ids, ids + sizeof( ids ) / sizeof( unsigned int ));

    const zeq::Event& selectionEvent =
        zeq::vocabulary::serializeIDSelection( idList );
    const std::vector< unsigned int >& deserializedSelection =
            zeq::vocabulary::deserializeIDSelection( selectionEvent );
    BOOST_CHECK_EQUAL_COLLECTIONS( idList.begin(), idList.end(),
                                   deserializedSelection.begin(),
                                   deserializedSelection.end( ));

    const zeq::Event& toggleEvent =
        zeq::vocabulary::serializeToggleIDsRequest( idList );
    const std::vector< unsigned int >& deserializedToggle =
            zeq::vocabulary::deserializeToggleIDsRequest( toggleEvent );
    BOOST_CHECK_EQUAL_COLLECTIONS( idList.begin(), idList.end(),
                                   deserializedToggle.begin(),
                                   deserializedToggle.end( ));
}

BOOST_AUTO_TEST_CASE(test_invalid_serialization)
{
    const std::vector< float > invalidCamera( 42 );
    const zeq::Event& event =
            zeq::vocabulary::serializeCamera( invalidCamera );
    BOOST_CHECK_EQUAL( event.getType(), zeq::vocabulary::EVENT_INVALID );
    BOOST_CHECK_EQUAL( event.getSize(), 0 );
}
