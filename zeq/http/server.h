
/* Copyright (c) 2016, Human Brain Project
 *                     Stefan.Eilemann@epfl.ch
 */

#ifndef ZEQ_HTTP_SERVER_H
#define ZEQ_HTTP_SERVER_H

#include <zeq/receiver.h> // base class

namespace zeq
{
/** HTTP protocol support. */
namespace http
{
/**
 * Serves HTTP GET and POST requests for servus::Serializable objects.
 *
 * Behaves semantically like a Publisher (for GET) and Subscriber (for POST),
 * except uses HTTP with JSON payload as the protocol. Requests are served
 * synchronously (as per HTTP spec).
 *
 * Not thread safe.
 *
 * Example: @include tests/http/server.cpp
 */
class Server : public zeq::Receiver
{
public:
    /**
     * Construct a new HTTP server.
     *
     * To process requests on the incoming port, call receive().
     *
     * @param uri The server address in the form "[tcp://][hostname][:port]"
     * @param shared a shared receiver, see Receiver constructor.
     * @throw std::runtime_error on malformed URI or connection issues.
     */
    ZEQ_API Server( const URI& uri, Receiver& shared );
    ZEQ_API explicit Server( const URI& uri );
    ZEQ_API explicit Server( Receiver& shared );
    ZEQ_API explicit Server();
    explicit Server( Server& shared )
        : Server( static_cast< Receiver& >( shared )) {}
    ZEQ_API virtual ~Server();

    /**
     * Create a new Server when requested.
     *
     * The creation and parameters depend on the following command line
     * parameters:
     * * --http-server [host][:port]: Enable the server. Optional parameters
     *   configure the web server, running by default on :4020
     */
    ZEQ_API static std::unique_ptr< Server > parse( int argc, char* argv[] );
    ZEQ_API static std::unique_ptr< Server > parse( int argc, char* argv[],
                                                    Receiver& shared );
    /**
     * Get the publisher URI.
     *
     * Contains the used hostname and port, if none where given in the
     * constructor uri.
     *
     * @return the publisher URI.
     * @todo change signature to return zeq::URI, needs downstream project
     *       adaptions. Also make zeq::URI( const servus::URI& from ) explicit.
     */
    ZEQ_API const servus::URI& getURI() const;

    /**
     * Subscribe a serializable object to receive updates from HTTP PUT
     * requests.
     *
     * Every update will be directly applied on the object during receive()
     * using fromJSON(). To track updates on the object, the serializable's
     * updated function is called accordingly.
     *
     * The subscribed object instance has to be valid until unsubscribe().
     *
     * @param object the object to update on receive()
     * @return true if subscription was successful, false otherwise
     */
    ZEQ_API bool subscribe( servus::Serializable& object );

    /** Unsubscribe the given object to stop applying updates. */
    ZEQ_API bool unsubscribe( const servus::Serializable& object );

    /**
     * Subscribe ther serializable object to serve HTTP GET requests.
     *
     * Every request will be directly handled during receive() by using
     * toJSON(). To track updates on the object, the serializable's received
     * function is called accordingly.
     *
     * The subscribed object instance has to be valid until unregister().
     *
     * @param object the object to serve during receive()
     * @return true if subscription was successful, false otherwise
     */
    ZEQ_API bool register_( servus::Serializable& object );

    /** Unsubscribe the given object for GET requests. */
    ZEQ_API bool unregister( const servus::Serializable& object );

private:
    class Impl;
    std::unique_ptr< Impl > _impl;

    // Receiver API
    void addSockets( std::vector< detail::Socket >& entries ) final;
    void process( detail::Socket& socket ) final;
    void addConnection( const std::string& ) final
    {
        throw std::runtime_error( "Add connection to HTTP server unsupported" );
    }
};
}
}

#endif
