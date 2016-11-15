
/* Copyright (c) 2016, Human Brain Project
 *                     Stefan.Eilemann@epfl.ch
 */

#ifndef ZEROEQ_HTTP_SERVER_H
#define ZEROEQ_HTTP_SERVER_H

#include <zeroeq/http/api.h>
#include <zeroeq/receiver.h> // base class
#include <zeroeq/log.h>

namespace zeroeq
{
/** HTTP protocol support. */
namespace http
{
/**
 * Serves HTTP GET and PUT requests for servus::Serializable objects.
 *
 * Behaves semantically like a Publisher (for GET) and Subscriber (for PUT),
 * except uses HTTP with JSON payload as the protocol. Requests are served
 * synchronously (as per HTTP spec). Objects are available under their
 * Serializable::getTypeName(), with '::' replaced by '/'. The REST API is case
 * insensitive. For example, zerobuf::render::Camera is served at
 * 'GET|PUT [uri]/zerobuf/render/camera'.
 *
 * Not thread safe.
 *
 * Example: @include tests/http/server.cpp
 */
class Server : public zeroeq::Receiver
{
public:
    /** @name Setup */
    //@{
    /**
     * Construct a new HTTP server.
     *
     * To process requests on the incoming port, call receive(). If no hostname
     * is given, the server listens on all interfaces (INADDR_ANY). If no port
     * is given, the server selects a random port. Use getURI() to retrieve the
     * chosen parameters.
     *
     * @param uri The server address in the form "[tcp://][hostname][:port]"
     * @param shared a shared receiver, see Receiver constructor.
     * @throw std::runtime_error on malformed URI or connection issues.
     */
    ZEROEQHTTP_API Server( const URI& uri, Receiver& shared );
    ZEROEQHTTP_API explicit Server( const URI& uri );
    ZEROEQHTTP_API explicit Server( Receiver& shared );
    ZEROEQHTTP_API Server();
    ZEROEQHTTP_API explicit Server( Server& shared )
        : Server( static_cast< Receiver& >( shared )) {}
    ZEROEQHTTP_API ~Server();

    /**
     * Create a new Server when requested.
     *
     * The creation and parameters depend on the following command line
     * parameters:
     * * --zeroeq-http-server [host][:port]: Enable the server. The optional
     *   parameters configure the web server, running by default on INADDR_ANY
     *   and a randomly chosen port
     */
    ZEROEQHTTP_API
    static std::unique_ptr< Server > parse( int argc, const char* const* argv);
    ZEROEQHTTP_API
    static std::unique_ptr< Server > parse( int argc, const char* const* argv,
                                            Receiver& shared );
    /**
     * Get the publisher URI.
     *
     * Contains the used hostname and port, if none where given in the
     * constructor uri.
     *
     * @return the publisher URI.
     */
    ZEROEQHTTP_API const URI& getURI() const;

    /**
     * Get the underlying socket descriptor.
     *
     * Can be used by client code to be notified when new data is available and
     * subsequently call receive. Due to implementation details, the socket
     * notifies on write, not on read.
     *
     * @return the socket descriptor.
     * @throw std::runtime_error if the descriptor could not be obtained.
     */
    ZEROEQHTTP_API SocketDescriptor getSocketDescriptor() const;
    //@}

    /** @name Object registration for PUT and GET requests */
    //@{
    /** Handle PUT and GET for the given object. */
    bool handle( servus::Serializable& object )
        { return handlePUT( object ) && handleGET( object );}

    /** Remove PUT and GET handling for given object. */
    ZEROEQHTTP_API bool remove( const servus::Serializable& object );

    /** Remove PUT and GET handling for given event. */
    ZEROEQHTTP_API bool remove( const std::string& event );

    /**
     * Subscribe a serializable object to receive updates from HTTP PUT
     * requests.
     *
     * Every update will be directly applied on the object during receive()
     * using fromJSON(). To track updates on the object, the serializable's
     * updated function is called accordingly.
     *
     * The subscribed object instance has to be valid until removePUT().
     *
     * @param object the object to update on receive()
     * @return true if subscription was successful, false otherwise
     */
    ZEROEQHTTP_API bool handlePUT( servus::Serializable& object );

    /**
     *
     * Subscribe an event to receive HTTP PUT requests.
     *
     * Every receival of the event will call the registered callback function.
     *
     * @param event the event name to receive PUT requests for during receive()
     * @param func the callback function for serving the PUT request
     * @return true if subscription was successful, false otherwise
     */
    ZEROEQHTTP_API
    bool handlePUT( const std::string& event, const PUTFunc& func );

    /**
     * @overload
     * @param event the event name to receive PUT requests for during receive()
     * @param schema describes data layout of event
     * @param func the callback function for serving the PUT request
     */
    ZEROEQHTTP_API bool handlePUT( const std::string& event,
                               const std::string& schema, const PUTFunc& func );

    /**
     * Subscribe an event to receive HTTP PUT requests with payload.
     *
     * Every receival of the event will call the registered callback function.
     *
     * @param event the event name to receive PUT requests for during receive()
     * @param func the callback function for serving the PUT request
     * @return true if subscription was successful, false otherwise
     */
    ZEROEQHTTP_API bool handlePUT( const std::string& event,
                               const PUTPayloadFunc& func );

    /**
     * @overload
     * @param event the event name to receive PUT requests for during receive()
     * @param schema describes data layout of event
     * @param func the callback function for serving the PUT request
     */
    ZEROEQHTTP_API bool handlePUT( const std::string& event,
                               const std::string& schema,
                               const PUTPayloadFunc& func );
    /**
     * Subscribe a serializable object to serve HTTP GET requests.
     *
     * Every request will be directly handled during receive() by using
     * toJSON(). To track updates on the object, the serializable's received
     * function is called accordingly.
     *
     * The subscribed object instance has to be valid until removeGET().
     *
     * @param object the object to serve during receive()
     * @return true if subscription was successful, false otherwise
     */
    ZEROEQHTTP_API bool handleGET( servus::Serializable& object );

    /**
     * Subscribe an event to serve HTTP GET requests.
     *
     * Every request will be directly handled during receive() by calling the
     * registered GET function.
     *
     * @param event the event name to serve during receive()
     * @param func the callback function for serving the GET request
     * @return true if subscription was successful, false otherwise
     */
    ZEROEQHTTP_API
    bool handleGET( const std::string& event, const GETFunc& func );

    /**
     * @overload
     * @param event the event name to serve during receive()
     * @param schema describes data layout of event
     * @param func the callback function for serving the GET request
     */
    ZEROEQHTTP_API
    bool handleGET( const std::string& event, const std::string& schema,
                    const GETFunc& func );

    /**
     * @return the registered schema for the given object, or empty if not
     *         registered.
     */
    ZEROEQHTTP_API
    std::string getSchema( const servus::Serializable& object) const;

    /** @overload */
    ZEROEQHTTP_API std::string getSchema( const std::string& event ) const;
    //@}

private:
    class Impl;
    std::unique_ptr< Impl > _impl;

    // Receiver API
    void addSockets( std::vector< detail::Socket >& entries ) final;
    void process( detail::Socket& socket, uint32_t timeout ) final;
    void addConnection( const std::string& ) final { ZEROEQDONTCALL; } // LCOV_EXCL_LINE
};
}
}

#endif
