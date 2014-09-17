
/* Copyright (c) 2014, Human Brain Project
 *                     Daniel Nachbaur <daniel.nachbaur@epfl.ch>
 *                     Stefan.Eilemann@epfl.ch
 */

#include "broker.h"

#include <boost/bind.hpp>

BOOST_AUTO_TEST_CASE(test_subscribe)
{
    zeq::Subscriber subscriber( test::buildURI( ));
}

BOOST_AUTO_TEST_CASE(test_invalid_subscribe)
{
    BOOST_CHECK_NO_THROW(
        zeq::Subscriber subscriber( lunchbox::URI( "bar://*" )));

    const lunchbox::URI& uri = test::buildURI();
    zeq::Subscriber subscriber( uri );
    BOOST_CHECK_THROW(
        zeq::Subscriber subscriber2( lunchbox::URI( "uri" )),
        std::runtime_error );
}

BOOST_AUTO_TEST_CASE(test_registerhandler)
{
    zeq::Subscriber subscriber( test::buildURI( ));
    BOOST_CHECK( subscriber.registerHandler( zeq::vocabulary::EVENT_CAMERA,
                                      boost::bind( &test::onCameraEvent, _1 )));
}

BOOST_AUTO_TEST_CASE(test_deregisterhandler)
{
    zeq::Subscriber subscriber( test::buildURI( ));
    BOOST_CHECK( subscriber.registerHandler( zeq::vocabulary::EVENT_CAMERA,
                                      boost::bind( &test::onCameraEvent, _1 )));
    BOOST_CHECK( subscriber.deregisterHandler( zeq::vocabulary::EVENT_CAMERA ));
}

BOOST_AUTO_TEST_CASE(test_invalid_registerhandler)
{
    zeq::Subscriber subscriber( test::buildURI( ));
    BOOST_CHECK( subscriber.registerHandler( zeq::vocabulary::EVENT_CAMERA,
                                      boost::bind( &test::onCameraEvent, _1 )));
    BOOST_CHECK( !subscriber.registerHandler( zeq::vocabulary::EVENT_CAMERA,
                                      boost::bind( &test::onCameraEvent, _1 )));
}

BOOST_AUTO_TEST_CASE(test_invalid_deregisterhandler)
{
    zeq::Subscriber subscriber( test::buildURI( ));
    BOOST_CHECK( !subscriber.deregisterHandler( zeq::vocabulary::EVENT_CAMERA));

    BOOST_CHECK( subscriber.registerHandler( zeq::vocabulary::EVENT_CAMERA,
                                      boost::bind( &test::onCameraEvent, _1 )));
    BOOST_CHECK( !subscriber.deregisterHandler(zeq::vocabulary::EVENT_INVALID));
}
