
/* Copyright (c) 2016-2017, Human Brain Project
 *                          Stefan.Eilemann@epfl.ch
 *                          Daniel.Nachbaur@epfl.ch
 */

#define BOOST_TEST_MODULE http_server

#include <zeroeq/http/server.h>
#include <zeroeq/subscriber.h>
#include <zeroeq/uri.h>
#include <servus/serializable.h>
#include <servus/uri.h>
#include <boost/test/unit_test.hpp>
#include <thread>

#include <boost/network/protocol/http/client.hpp>
#include <boost/network/protocol/http/server.hpp>

namespace
{
namespace http = boost::network::http;
// default client from cppnetlib is asynchronous, not handy for tests
using HTTPClient =
    http::basic_client< http::tags::http_default_8bit_tcp_resolve, 1, 1 >;

using ServerReponse = http::basic_response<http::tags::http_server>;

struct Response
{
    const ServerReponse::status_type status;
    const std::string body;
};

Response _buildResponse( const std::string& body = std::string( ))
{
    return { ServerReponse::ok, body };
}

const Response response200{ ServerReponse::ok, "" };
const Response error400{ ServerReponse::bad_request, "" };
const Response error404{ ServerReponse::not_found, "" };
const Response error405{ ServerReponse::method_not_allowed, "" };

const std::string jsonGet( "Not JSON, just want to see that the is data a-ok" );
const std::string jsonPut( "See what my stepbrother jsonGet says" );

class Foo : public servus::Serializable
{
public:
    Foo() { _notified = false; }

    void setNotified( const bool notified = true ) { _notified = notified; }
    bool getNotified() const { return _notified; }

    std::string getSchema() const final
    {
        return "{\n  '_notified' : 'bool'\n}";
    }

private:
    std::string getTypeName() const final { return "test::Foo"; }
    virtual zeroeq::uint128_t getTypeIdentifier() const final
        { return servus::make_uint128( getTypeName( )); }

    bool _fromJSON( const std::string& json ) final
    {
        return jsonPut == json;
    }

    std::string _toJSON() const final
    {
        return jsonGet;
    }

    bool _notified;
};


class Client : private HTTPClient
{
public:
    Client( const zeroeq::URI& uri )
    {
        std::ostringstream url;
        url << "http://" << uri.getHost() << ":" << uri.getPort();
        _baseURL = url.str();
    }

    ~Client()
    {
    }

    void checkGET( const std::string& request,
                   const Response& expected, const int line )
    {
        _checkImpl( Method::GET, request, "", expected, line );
    }

    void checkPUT( const std::string& request, const std::string& data,
                   const Response& expected, const int line )
    {
        _checkImpl( Method::PUT, request, data, expected, line );
    }

    void checkPOST( const std::string& request, const std::string& data,
                    const Response& expected, const int line )
    {
        _checkImpl( Method::POST, request, data, expected, line );
    }

    void sendGET( const std::string& request )
    {
        HTTPClient::request request_( _baseURL + request );
        get( request_ );
    }

private:
    std::string _baseURL;

    enum class Method
    {
        GET,
        POST,
        PUT
    };

    void _checkImpl( const Method method, const std::string& request,
                     const std::string& data,
                     const Response& expected, const int line )
    {
        HTTPClient::request request_( _baseURL + request );

        HTTPClient::response response;
        switch( method )
        {
        case Method::GET:
            response = get( request_ );
            break;
        case Method::POST:
            response = post( request_, data );
            break;
        case Method::PUT:
            response = put( request_, data );
            break;
        }

        BOOST_CHECK_MESSAGE( status( response ) == expected.status,
                             "At l." + std::to_string( line ) + ": " +
                             std::to_string( status( response ) )+ " != " +
                             std::to_string( int(expected.status) )  );

        std::map< std::string, std::string > expectedHeaders;
        expectedHeaders.insert( { "Access-Control-Allow-Headers",
                                  "Content-Type" });
        expectedHeaders.insert( { "Access-Control-Allow-Methods",
                                  "GET,PUT,OPTIONS" });
        expectedHeaders.insert( { "Access-Control-Allow-Origin", "*" });

        if( !expected.body.empty( ))
        {
            expectedHeaders.insert( std::make_pair( "Content-Length",
                               std::to_string( expected.body.size( ))));
        }

        const auto& responseHeaders = headers( response );
        const size_t responseHeaderSize =
                std::distance( responseHeaders.begin(), responseHeaders.end( ));
        BOOST_REQUIRE_MESSAGE( responseHeaderSize == expectedHeaders.size(),
                               "At l." + std::to_string( line ) + ": " +
                               std::to_string( responseHeaderSize )+ " != " +
                               std::to_string( expectedHeaders.size( )));
        auto i = responseHeaders.begin();
        auto j = expectedHeaders.begin();
        for( ; i != responseHeaders.end(); ++i, ++j )
        {
            BOOST_CHECK_EQUAL( i->first, j->first );
            BOOST_CHECK_EQUAL( i->second, j->second );
        }

        const auto& responseBody = body( response );
        auto responseBodyString = static_cast< std::string >( responseBody );
        // weird stuff here: cppnetlib client response body string is
        // duplicated, hence take first half for comparison
        responseBodyString.erase( responseBodyString.size() / 2,
                                  responseBodyString.size() / 2 );
        BOOST_CHECK_MESSAGE( responseBodyString == expected.body,
                             "At l." + std::to_string( line ) + ": " +
                             responseBodyString + " != " + expected.body );
    }
};

}

BOOST_AUTO_TEST_CASE(construction)
{
    zeroeq::http::Server server1;
    BOOST_CHECK_NE( server1.getURI().getHost(), "" );
    BOOST_CHECK_NE( server1.getURI().getHost(), "*" );
    BOOST_CHECK_NE( server1.getURI().getPort(), 0 );
    BOOST_CHECK_NO_THROW( server1.getSocketDescriptor( ));
    BOOST_CHECK_GT( server1.getSocketDescriptor(), 0 );

    const zeroeq::URI uri( "tcp://" );
    zeroeq::http::Server server2( uri );
    zeroeq::http::Server server3( uri );
    BOOST_CHECK_NE( server2.getURI(), server3.getURI( ));
    BOOST_CHECK_NE( server2.getURI().getPort(), 0 );

    BOOST_CHECK_THROW( zeroeq::http::Server( server2.getURI( )),
                       std::runtime_error );
    BOOST_CHECK_NO_THROW( server2.getSocketDescriptor( ));
    BOOST_CHECK_GT( server1.getSocketDescriptor(), 0 );
}

BOOST_AUTO_TEST_CASE(construction_argv_host_port)
{
    const char* app = boost::unit_test::framework::master_test_suite().argv[0];
    const char* argv[] = { app, "--zeroeq-http-server", "127.0.0.1:0" };
    const int argc = sizeof(argv)/sizeof(char*);

    std::unique_ptr< zeroeq::http::Server > server1 =
            zeroeq::http::Server::parse( argc, argv );

    BOOST_REQUIRE( server1 );
    BOOST_CHECK_EQUAL( server1->getURI().getHost(), "127.0.0.1" );
    BOOST_CHECK_NE( server1->getURI().getPort(), 0 );

    zeroeq::Subscriber shared;
    std::unique_ptr< zeroeq::http::Server > server2 =
            zeroeq::http::Server::parse( argc, argv, shared );

    BOOST_REQUIRE( server2 );
    BOOST_CHECK_EQUAL( server2->getURI().getHost(), "127.0.0.1" );
    BOOST_CHECK_NE( server2->getURI().getPort(), 0 );
}

BOOST_AUTO_TEST_CASE(construction_argv)
{
    const char* app = boost::unit_test::framework::master_test_suite().argv[0];
    const char* argv[] = { app, "--zeroeq-http-server" };
    const int argc = sizeof(argv)/sizeof(char*);

    std::unique_ptr< zeroeq::http::Server > server1 =
            zeroeq::http::Server::parse( argc, argv );

    BOOST_REQUIRE( server1 );
    BOOST_CHECK( !server1->getURI().getHost().empty( ));
    BOOST_CHECK_NE( server1->getURI().getPort(), 0 );

    zeroeq::Subscriber shared;
    std::unique_ptr< zeroeq::http::Server > server2 =
            zeroeq::http::Server::parse( argc, argv, shared );

    BOOST_CHECK( !server2->getURI().getHost().empty( ));
    BOOST_CHECK_NE( server2->getURI().getPort(), 0 );
}

BOOST_AUTO_TEST_CASE(construction_empty_argv)
{
    const char* app = boost::unit_test::framework::master_test_suite().argv[0];
    const char* argv[] = { app };
    const int argc = sizeof(argv)/sizeof(char*);

    std::unique_ptr< zeroeq::http::Server > server1 =
            zeroeq::http::Server::parse( argc, argv );

    BOOST_CHECK( !server1 );

    zeroeq::Subscriber shared;
    std::unique_ptr< zeroeq::http::Server > server2 =
            zeroeq::http::Server::parse( argc, argv, shared );

    BOOST_CHECK( !server2 );
}

BOOST_AUTO_TEST_CASE(registration)
{
    zeroeq::http::Server server;
    Foo foo;
    BOOST_CHECK( server.handleGET( foo ));
    BOOST_CHECK( !server.handleGET( foo ));
    BOOST_CHECK( server.remove( foo ));
    BOOST_CHECK( !server.remove( foo ));

    BOOST_CHECK( server.handleGET( "foo", [](){ return "bla"; } ));
    BOOST_CHECK( !server.handleGET( "foo", [](){ return "bla"; } ));
    BOOST_CHECK( server.remove( "foo" ));
    BOOST_CHECK( !server.remove( "foo" ));

    BOOST_CHECK( server.handleGET( "bar", "schema", [](){ return "bla"; } ));
    BOOST_CHECK( !server.handleGET( "bar", "schema", [](){ return "bla"; } ));
    BOOST_CHECK( server.remove( "bar" ));
    BOOST_CHECK( !server.remove( "bar" ));

    BOOST_CHECK( server.handlePUT( foo ));
    BOOST_CHECK( !server.handlePUT( foo ));
    BOOST_CHECK( server.remove( foo ));
    BOOST_CHECK( !server.remove( foo ));

    BOOST_CHECK( server.handlePUT( "foo", zeroeq::PUTPayloadFunc
                                  ([]( const std::string& ) { return true; })));
    BOOST_CHECK( !server.handlePUT( "foo", zeroeq::PUTPayloadFunc
                                  ([]( const std::string& ) { return true; })));
    BOOST_CHECK( server.remove( "foo" ));
    BOOST_CHECK( !server.remove( "foo" ));

    BOOST_CHECK( server.handlePUT( "bar", "schema", zeroeq::PUTPayloadFunc
                                  ([]( const std::string& ) { return true; })));
    BOOST_CHECK( !server.handlePUT( "bar", "schema", zeroeq::PUTPayloadFunc
                                  ([]( const std::string& ) { return true; })));
    BOOST_CHECK_EQUAL( server.getSchema( "bar" ), "schema" );
    BOOST_CHECK( server.handleGET( "bar", "schema", [](){ return "bla"; } ));
    BOOST_CHECK( !server.handleGET( "bar", "schema", [](){ return "bla"; } ));
    BOOST_CHECK_EQUAL( server.getSchema( "bar" ), "schema" );
    BOOST_CHECK( server.remove( "bar" ));
    BOOST_CHECK( !server.remove( "bar" ));
    BOOST_CHECK_EQUAL( server.getSchema( "bar" ), "" );

    BOOST_CHECK( server.handle( foo ));
    BOOST_CHECK( !server.handle( foo ));
    BOOST_CHECK( server.remove( foo ));
    BOOST_CHECK( !server.remove( foo ));

    BOOST_CHECK( server.handle( foo ));
    BOOST_CHECK_EQUAL( server.getSchema( foo ), foo.getSchema( ));
    BOOST_CHECK( server.remove( foo ));
    BOOST_CHECK_EQUAL( server.getSchema( foo ), std::string( ));
}

BOOST_AUTO_TEST_CASE(get_serializable)
{
    bool running = true;
    zeroeq::http::Server server;
    Foo foo;

    foo.registerSerializeCallback( [&]{ foo.setNotified(); });
    server.handleGET( foo );

    std::thread thread( [&]() { while( running ) server.receive( 100 ); });

    Client client( server.getURI( ));

    client.checkGET( "/unknown", error404, __LINE__ );
    BOOST_CHECK( !foo.getNotified( ));

    client.checkGET( "/test/Foo", _buildResponse( jsonGet ), __LINE__ );
    BOOST_CHECK( foo.getNotified( ));

    running = false;
    thread.join();
}

BOOST_AUTO_TEST_CASE(get_event)
{
    bool running = true;
    zeroeq::http::Server server;

    bool requested = false;
    server.handleGET( "test::Foo", [&](){ requested = true; return jsonGet; } );

    std::thread thread( [&]() { while( running ) server.receive( 100 ); });

    Client client( server.getURI( ));

    client.checkGET( "/unknown", error404, __LINE__ );
    BOOST_CHECK( !requested );

    // regression check for bugfix #190 (segfault with GET '/')
    client.checkGET( "/", error404, __LINE__ );
    BOOST_CHECK( !requested );

    client.checkGET( "/test/Foo", _buildResponse( jsonGet ), __LINE__ );
    BOOST_CHECK( requested );

    running = false;
    thread.join();
}

BOOST_AUTO_TEST_CASE(shared)
{
    bool running = true;
    zeroeq::Subscriber subscriber;
    zeroeq::http::Server server1( subscriber );
    zeroeq::http::Server server2( server1 );
    Foo foo;
    server2.handleGET( foo );

    std::thread thread( [&]() { while( running ) subscriber.receive( 100 );});

    Client client1( server1.getURI( ));
    Client client2( server2.getURI( ));

    client1.checkGET( "/test/Foo", error404, __LINE__ );
    client2.checkGET( "/test/Foo", _buildResponse( jsonGet ), __LINE__ );

    running = false;
    thread.join();
}

BOOST_AUTO_TEST_CASE(put_serializable)
{
    bool running = true;
    zeroeq::http::Server server;
    Foo foo;

    foo.registerDeserializedCallback( [&]{ foo.setNotified(); });
    server.handlePUT( foo );

    std::thread thread( [&]() { while( running ) server.receive( 100 ); });

    Client client( server.getURI( ));
    client.checkPUT( "/test/Foo", "", error400, __LINE__ );
    client.checkPUT( "/test/Foo", "Foo", error400, __LINE__ );
    client.checkPUT( "/test/Bar", jsonPut, error404, __LINE__ );
    BOOST_CHECK( !foo.getNotified( ));

    client.checkPUT( "/test/Foo", jsonPut, response200, __LINE__ );
    BOOST_CHECK( foo.getNotified( ));

    running = false;
    thread.join();
}

BOOST_AUTO_TEST_CASE(put_event)
{
    bool running = true;
    zeroeq::http::Server server;

    bool receivedEmpty = false;
    server.handlePUT( "empty", zeroeq::PUTFunc
                               ([&]() { receivedEmpty = true; return true; } ));
    server.handlePUT( "foo", zeroeq::PUTPayloadFunc
                             ([&]( const std::string& received )
                             { return jsonPut == received; } ));

    std::thread thread( [&]() { while( running ) server.receive( 100 ); });

    Client client( server.getURI( ));
    client.checkPUT( "/foo", "", error400, __LINE__ );
    client.checkPUT( "/foo", "Foo", error400, __LINE__ );
    client.checkPUT( "/test/Bar", jsonPut, error404, __LINE__ );
    client.checkPUT( "/foo", jsonPut, response200, __LINE__ );
    client.checkPUT( "/empty", " ", response200, __LINE__ );
    BOOST_CHECK( receivedEmpty );

    running = false;
    thread.join();
}

BOOST_AUTO_TEST_CASE(post)
{
    bool running = true;
    zeroeq::http::Server server;
    Foo foo;
    server.handleGET( foo );

    std::thread thread( [&]() { while( running ) server.receive( 100 ); });

    Client client( server.getURI( ));
    client.checkPOST( "/test/Foo", jsonPut, error405, __LINE__ );

    running = false;
    thread.join();
}

BOOST_AUTO_TEST_CASE(largeGet)
{
    bool running = true;
    zeroeq::http::Server server;
    Foo foo;
    server.handleGET( foo );

    std::thread thread( [&]() { while( running ) server.receive( 100 ); });

    Client client( server.getURI( ));
    const auto request = std::string( "/test/Foo?" ) + std::string( 4096, 'o' );
    client.checkGET( request, _buildResponse( jsonGet ), __LINE__ );

    running = false;
    thread.join();
}

BOOST_AUTO_TEST_CASE(issue157)
{
    bool running = true;
    zeroeq::http::Server server;
    Foo foo;
    server.handleGET( foo );

    std::thread thread( [&]() { while( running ) server.receive( 100 ); });

    // Close client before receiving request to provoke #157
    {
        Client client( server.getURI( ));
        const auto request = std::string( "/test/Foo?" ) +
                             std::string( 4096, 'o' );
        client.sendGET( request );
    }

    running = false;
    thread.join();
}

BOOST_AUTO_TEST_CASE(urlcasesensitivity)
{
    bool running = true;
    zeroeq::http::Server server;
    Foo foo;
    server.handleGET( foo );

    std::thread thread( [&]() { while( running ) server.receive( 100 ); });

    Client client( server.getURI( ));
    client.checkGET( "/TEST/FOO", _buildResponse( jsonGet ), __LINE__ );
    client.checkGET( "/test/foo", _buildResponse( jsonGet ), __LINE__ );

    running = false;
    thread.join();
}

BOOST_AUTO_TEST_CASE(empty_registry)
{
    zeroeq::http::Server server;
    bool running = true;
    std::thread thread( [&]() { while( running ) server.receive( 100 ); });

    Client client( server.getURI( ));
    client.checkGET( "/registry", _buildResponse( "{}\n" ), __LINE__ );

    running = false;
    thread.join();
}

BOOST_AUTO_TEST_CASE(filled_registry)
{
    zeroeq::http::Server server;
    Foo foo;
    server.handle( foo );
    server.handlePUT( "bla/bar", zeroeq::PUTFunc( [] { return true; } ));

    bool running = true;
    std::thread thread( [&]() { while( running ) server.receive( 100 ); });

    Client client( server.getURI( ));
    client.checkGET( "/registry",
                     _buildResponse( "{\n   \"bla/bar\" : [ \"PUT\" ],\n"\
                                "   \"test/foo\" : [ \"GET\", \"PUT\" ]\n}\n" ),
                                     __LINE__ );

    client.checkGET( "/bla/bar/registry", error404, __LINE__ );

    running = false;
    thread.join();
}

BOOST_AUTO_TEST_CASE(object_schema)
{
    zeroeq::http::Server server;
    Foo foo;
    server.handleGET( foo );

    bool running = true;
    std::thread thread( [&]() { while( running ) server.receive( 100 ); });

    Client client( server.getURI( ));
    client.checkGET( "/test/foo/schema",
                     _buildResponse( "{\n  '_notified' : 'bool'\n}" ),
                     __LINE__ );
    client.checkGET( "/test/foo/schema/schema", error404, __LINE__ );

    running = false;
    thread.join();
}

BOOST_AUTO_TEST_CASE(event_schema)
{
    zeroeq::http::Server server;
    const std::string schema = "{ \"value\" : \"boolean\" }";
    server.handleGET( "bla/bar", schema,
        zeroeq::GETFunc( [] { return std::string( "{ \"value\" : true }"); } ));
    server.handlePUT( "bla/foo", schema,
                      zeroeq::PUTFunc( [] { return true; } ));

    bool running = true;
    std::thread thread( [&]() { while( running ) server.receive( 100 ); });

    Client client( server.getURI( ));
    client.checkGET( "/bla/bar/schema", _buildResponse( schema ), __LINE__ );
    client.checkGET( "/bla/foo/schema", _buildResponse( schema ), __LINE__ );

    running = false;
    thread.join();
}

BOOST_AUTO_TEST_CASE(event_no_schema)
{
    zeroeq::http::Server server;
    server.handleGET( "bla/bar",
        zeroeq::GETFunc( [] { return std::string( "{ \"value\" : true }"); } ));

    bool running = true;
    std::thread thread( [&] { while( running ) server.receive( 100 ); });

    Client client( server.getURI( ));
    client.checkGET( "/bla/bar/schema", error404, __LINE__ );

    running = false;
    thread.join();
}

BOOST_AUTO_TEST_CASE(event_wrong_schema)
{
    zeroeq::http::Server server;
    BOOST_CHECK( server.handlePUT( "bla/foo", "schema",
                                   zeroeq::PUTFunc( [] { return true; } )));
    BOOST_CHECK_THROW( server.handleGET( "bla/foo", "bad",
                               zeroeq::GETFunc( [] { return std::string(); } )),
                       std::runtime_error );

    BOOST_CHECK( server.handleGET( "bar", "schema",
                              zeroeq::GETFunc( [] { return std::string(); } )));
    BOOST_CHECK_THROW( server.handlePUT( "bar", "bad",
                                zeroeq::PUTFunc( [] { return true; } )),
                       std::runtime_error );
}

BOOST_AUTO_TEST_CASE(event_registry_name)
{
    zeroeq::http::Server server;
    BOOST_CHECK_THROW( server.handleGET( "registry",
                               zeroeq::GETFunc( [] { return std::string(); } )),
                       std::runtime_error );
    BOOST_CHECK_THROW( server.handlePUT( "registry",
                                zeroeq::PUTFunc( [] { return true; } )),
                       std::runtime_error );

    BOOST_CHECK_THROW( server.handleGET( "",
                               zeroeq::GETFunc( [] { return std::string(); } )),
                       std::runtime_error );
    BOOST_CHECK_THROW( server.handlePUT( "",
                                zeroeq::PUTFunc( [] { return true; } )),
                       std::runtime_error );

    BOOST_CHECK( server.handleGET( "foo/registry",
                       zeroeq::GETFunc( [] { return std::string( "bar" ); } )));
    BOOST_CHECK( server.handlePUT( "foo/registry",
                                   zeroeq::PUTFunc( [] { return true; } )));

    bool running = true;
    std::thread thread( [&]() { while( running ) server.receive( 100 ); });

    Client client( server.getURI( ));
    client.checkGET( "/foo/registry", _buildResponse( "bar" ), __LINE__ );

    running = false;
    thread.join();
}

BOOST_AUTO_TEST_CASE(event_schema_name)
{
    zeroeq::http::Server server;
    BOOST_CHECK( server.handleGET( "schema", "dummy_schema",
                       zeroeq::GETFunc( [] { return std::string( "bar" ); } )));
    BOOST_CHECK( server.handlePUT( "schema", "dummy_schema",
                                   zeroeq::PUTFunc( [] { return true; } )));


    bool running = true;
    std::thread thread( [&]() { while( running ) server.receive( 100 ); });

    Client client( server.getURI( ));
    client.checkGET( "/schema", _buildResponse( "bar" ), __LINE__ );
    client.checkGET( "/schema/schema", _buildResponse( "dummy_schema" ),
                     __LINE__ );

    running = false;
    thread.join();
}

BOOST_AUTO_TEST_CASE(multiple_event_name_for_same_object)
{
    bool running = true;
    zeroeq::http::Server server;
    Foo foo;

    foo.registerSerializeCallback( [&]{ foo.setNotified(); });
    foo.registerDeserializedCallback( [&]{ foo.setNotified(); });

    server.handleGET( foo );
    server.handlePUT( "test/camelBar", foo );

    std::thread thread( [&]() { while( running ) server.receive( 100 ); });

    Client client( server.getURI( ));

    client.checkPUT( "/test/camel-bar", jsonPut, response200, __LINE__ );
    BOOST_CHECK( foo.getNotified( ));

    client.checkPUT( "/test/foo", "", error404, __LINE__ );

    foo.setNotified( false );

    client.checkGET( "/test/foo", _buildResponse( jsonGet ), __LINE__ );
    BOOST_CHECK( foo.getNotified( ));

    client.checkGET( "/test/camel-bar", error404, __LINE__ );

    running = false;
    thread.join();
}
