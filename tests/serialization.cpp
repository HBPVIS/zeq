
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

    unsigned int ids[] = {16,2,77,29};
    const std::vector< unsigned int > selection( ids, ids + sizeof(ids) / sizeof(unsigned int) );
    const zeq::Event& selection_event = zeq::vocabulary::serializeIDSelection( selection );
    const std::vector< unsigned int >& deserialized_selection =
            zeq::vocabulary::deserializeIDSelection( selection_event );
    BOOST_CHECK_EQUAL_COLLECTIONS( selection.begin(), selection.end(),
                                   deserialized_selection.begin(), deserialized_selection.end( ));

}

BOOST_AUTO_TEST_CASE(test_invalid_serialization)
{
    const std::vector< float > invalidCamera( 42 );
    const zeq::Event& event =
            zeq::vocabulary::serializeCamera( invalidCamera);
    BOOST_CHECK_EQUAL( event.getType(), zeq::vocabulary::EVENT_INVALID );
    BOOST_CHECK_EQUAL( event.getSize(), 0 );
}
