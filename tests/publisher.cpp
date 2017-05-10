
/* Copyright (c) 2015-2016, Human Brain Project
 *                          Daniel Nachbaur <daniel.nachbaur@epfl.ch>
 *                          Stefan.Eilemann@epfl.ch
 *                          Juan Hernando <jhernando@fi.upm.es>
 */

#define BOOST_TEST_MODULE zeroeq_publisher

#include "broker.h"
#include <zeroeq/detail/broker.h>
#include <zeroeq/detail/constants.h>
#include <zeroeq/detail/sender.h>

#include <servus/servus.h>

#ifdef _MSC_VER
#define setenv(name, value, overwrite) _putenv_s(name, value)
#define unsetenv(name) _putenv_s(name, nullptr)
#endif

BOOST_AUTO_TEST_CASE(create_uri_publisher)
{
    const zeroeq::Publisher publisher(zeroeq::URI(""));

    const zeroeq::URI& uri = publisher.getURI();
    const std::string expectedScheme("tcp");
    const std::string baseScheme =
        uri.getScheme().substr(0, expectedScheme.length());
    BOOST_CHECK_EQUAL(baseScheme, expectedScheme);
    BOOST_CHECK(!uri.getHost().empty());
    BOOST_CHECK(uri.getPort() > 1024);
}

BOOST_AUTO_TEST_CASE(create_invalid_uri_publisher)
{
    // invalid URI, hostname only not allowed
    BOOST_CHECK_THROW(zeroeq::Publisher publisher(zeroeq::URI("localhost")),
                      std::runtime_error);
}

BOOST_AUTO_TEST_CASE(publish)
{
    zeroeq::Publisher publisher(zeroeq::NULL_SESSION);
    test::Echo echo(test::echoMessage);
    BOOST_CHECK(publisher.publish(echo));
    BOOST_CHECK(publisher.publish(echo.getTypeIdentifier(),
                                  echo.toBinary().ptr.get(),
                                  echo.toBinary().size));
}

BOOST_AUTO_TEST_CASE(publish_update_uri)
{
    zeroeq::Publisher publisher(zeroeq::NULL_SESSION);
    const zeroeq::URI& uri = publisher.getURI();
    BOOST_CHECK_MESSAGE(uri.getPort() != 0, uri);
    BOOST_CHECK_MESSAGE(!uri.getHost().empty(), uri);
    BOOST_CHECK(publisher.publish(test::Echo(test::echoMessage)));
}

BOOST_AUTO_TEST_CASE(publish_empty_event)
{
    zeroeq::Publisher publisher(zeroeq::NULL_SESSION);
    BOOST_CHECK(publisher.publish(test::Empty()));
    BOOST_CHECK(publisher.publish(zeroeq::make_uint128("Empty")));
}

BOOST_AUTO_TEST_CASE(multiple_publisher_on_same_host)
{
    if (!servus::Servus::isAvailable() || getenv("TRAVIS"))
        return;

    const zeroeq::Publisher publisher1;
    const zeroeq::Publisher publisher2;
    const zeroeq::Publisher publisher3;

    servus::Servus service(PUBLISHER_SERVICE);
    const servus::Strings& instances =
        service.discover(servus::Servus::IF_LOCAL, 1000);
    BOOST_CHECK_EQUAL(instances.size(), 3);
}

BOOST_AUTO_TEST_CASE(zeroconf_record)
{
    if (!servus::Servus::isAvailable() || getenv("TRAVIS"))
        return;

    const zeroeq::Publisher publisher;

    servus::Servus service(PUBLISHER_SERVICE);
    const servus::Strings& instances =
        service.discover(servus::Servus::IF_LOCAL, 1000);
    BOOST_REQUIRE_EQUAL(instances.size(), 1);

    const std::string& instance = instances[0];
    BOOST_CHECK_EQUAL(instance, publisher.getAddress());
    BOOST_CHECK_EQUAL(service.get(instance, KEY_APPLICATION), "publisher");
    BOOST_CHECK_EQUAL(zeroeq::uint128_t(service.get(instance, KEY_INSTANCE)),
                      zeroeq::detail::Sender::getUUID());
    BOOST_CHECK_EQUAL(service.get(instance, KEY_SESSION), getUserName());
    BOOST_CHECK_EQUAL(service.get(instance, KEY_USER), getUserName());
}

BOOST_AUTO_TEST_CASE(custom_session)
{
    if (!servus::Servus::isAvailable() || getenv("TRAVIS"))
        return;

    const zeroeq::Publisher publisher(test::buildUniqueSession());

    servus::Servus service(PUBLISHER_SERVICE);
    const servus::Strings& instances =
        service.discover(servus::Servus::IF_LOCAL, 1000);
    BOOST_REQUIRE_EQUAL(instances.size(), 1);

    const std::string& instance = instances[0];
    BOOST_CHECK_EQUAL(service.get(instance, KEY_SESSION),
                      publisher.getSession());
}

BOOST_AUTO_TEST_CASE(different_session_at_runtime)
{
    if (!servus::Servus::isAvailable() || getenv("TRAVIS"))
        return;

    setenv("ZEROEQ_SESSION", "testsession", 1);
    const zeroeq::Publisher publisher;

    servus::Servus service(PUBLISHER_SERVICE);
    const servus::Strings& instances =
        service.discover(servus::Servus::IF_LOCAL, 1000);
    BOOST_REQUIRE_EQUAL(instances.size(), 1);

    const std::string& instance = instances[0];
    BOOST_CHECK_EQUAL(service.get(instance, KEY_SESSION), "testsession");
    unsetenv("ZEROEQ_SESSION");
}

BOOST_AUTO_TEST_CASE(empty_session)
{
    BOOST_CHECK_THROW(const zeroeq::Publisher publisher(""),
                      std::runtime_error);
}

BOOST_AUTO_TEST_CASE(empty_session_from_environment)
{
    setenv("ZEROEQ_SESSION", "", 1);

    const zeroeq::Publisher publisher;
    BOOST_CHECK_EQUAL(publisher.getSession(), getUserName());

    unsetenv("ZEROEQ_SESSION");
}

BOOST_AUTO_TEST_CASE(fixed_uri_and_session)
{
    if (!servus::Servus::isAvailable() || getenv("TRAVIS"))
        return;

    const zeroeq::Publisher publisher(zeroeq::URI("127.0.0.1"),
                                      test::buildUniqueSession());
    servus::Servus service(PUBLISHER_SERVICE);
    const servus::Strings& instances =
        service.discover(servus::Servus::IF_LOCAL, 1000);
    BOOST_REQUIRE_EQUAL(instances.size(), 1);
}
