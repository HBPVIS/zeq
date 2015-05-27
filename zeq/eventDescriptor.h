
/* Copyright (c) 2015, Human Brain Project
 *                     grigori.chevtchenko@epfl.ch
 */


#ifndef ZEQ_EVENTDESCRIPTOR_H
#define ZEQ_EVENTDESCRIPTOR_H

#include <zeq/api.h>

#include <zeq/types.h>

namespace zeq
{
namespace detail { struct EventDescriptor; }

/**
 * This enum holds information about the Event "direction" (only Published,
 * only Subscribed or Bidirectional) from the application point of view.
 */
enum EventDirection
{
    SUBSCRIBER = 0,
    PUBLISHER,
    BIDIRECTIONAL
};

/**
 * This structure holds informations about an Event from a vocabulary.
 * It contains the REST api name, the 128bit zeq event id and the schema
 * needed for automatic JSON serialization/deserialization.
 */
struct EventDescriptor
{
    /**
     * Create an EventDescriptor.
     * @param restName the string used for REST command
     * @param eventType the zeq event's uint128
     * @param schema the flatbuffers schema as string
     * @param eventDirection receiving, sending, or both
     */
    ZEQ_API EventDescriptor( const std::string& restName,
                             const uint128_t& eventType,
                             const std::string& schema,
                             const EventDirection eventDirection );

#ifdef BOOST_NO_CXX11_RVALUE_REFERENCES
    /** Copy ctor @internal */
    ZEQ_API EventDescriptor( const EventDescriptor& rhs )
        : _impl(0)
    {
        copy(rhs);
    }

    /** Assignment operator @internal */
    ZEQ_API EventDescriptor& operator=( const EventDescriptor& rhs )
    {
        copy( rhs );
        return *this;
    }
#else
    /** Move ctor @internal */
    ZEQ_API EventDescriptor( EventDescriptor&& rhs );
#endif

    ZEQ_API ~EventDescriptor();

    /** @return the REST command string of this event */
    ZEQ_API const std::string& getRestName() const;

    /** @return the zeq event's uint128 */
    ZEQ_API const uint128_t& getEventType() const;

    /** @return the flatbuffers schema string*/
    ZEQ_API const std::string& getSchema() const;

    /** @return the zeq event's direction (Subscribed, Pulished or both)*/
    ZEQ_API EventDirection getEventDirection() const;

private:
    EventDescriptor( const EventDescriptor& ) = delete;
    EventDescriptor& operator=( const EventDescriptor& ) = delete;

    void copy( const EventDescriptor& rhs );

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    EventDescriptor& operator=( EventDescriptor&& rhs );
#endif
    detail::EventDescriptor* _impl;
};

}

#endif
