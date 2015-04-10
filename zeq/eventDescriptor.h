
/* Copyright (c) 2015, Human Brain Project
 *                     grigori.chevtchenko@epfl.ch
 */


#ifndef ZEQ_EVENTDESCRIPTOR_H
#define ZEQ_EVENTDESCRIPTOR_H

#include <zeq/api.h>

#include <zeq/types.h>
#include <boost/noncopyable.hpp>

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
#ifdef SUPPORT_FOR_CPP11
    : public boost::noncopyable
#endif
{
    /**
     * Create an EventDescriptor.
     * @param restName the string used for REST command
     * @param eventType the zeq event's uint128
     * @param schema the flatbuffers schema as string
     */
    ZEQ_API EventDescriptor( const std::string& restName,
                             const uint128_t& eventType,
                             const std::string& schema,
                             const EventDirection eventDirection);

    /** Move ctor @internal */
#ifdef SUPPORT_FOR_CPP11
    ZEQ_API EventDescriptor( EventDescriptor&& rhs );
#else
    ZEQ_API EventDescriptor( const EventDescriptor& rhs )
    {
        _impl = rhs._impl;
        const_cast<EventDescriptor&>(rhs)._impl = 0;
    }

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

#ifndef SUPPORT_FOR_CPP11
    EventDescriptor& operator=( const EventDescriptor& rhs )
    {
        _impl = rhs._impl;
        const_cast<EventDescriptor&>(rhs)._impl = 0;
        return *this;
    }
#endif

private:

#ifdef SUPPORT_FOR_CPP11
    EventDescriptor& operator=( EventDescriptor&& rhs );
#endif
    detail::EventDescriptor* _impl;
};

}

#endif
