
/* Copyright (c) 2014-2015, Human Brain Project
 *                          Daniel Nachbaur <daniel.nachbaur@epfl.ch>
 */

#ifndef ZEQ_EVENT_H
#define ZEQ_EVENT_H

#include <zeq/api.h>
#include <zeq/types.h>

namespace zeq
{
namespace detail{ class Subscriber; class Event; }

/**
 * An event is emitted by a Publisher to notify Subscriber of a change.
 *
 * Events are published via zeq::Publisher and received via zeq::Subscriber. The
 * format of the serialized data is specific to the serialization backend.
 *
 * Example: @include tests/serialization.cpp
 */
class Event
{
public:
    /**
     * Construct a new event of the given type
     *
     * @param type the desired event type
     * @sa vocabulary::registerEvent
     */
    ZEQ_API explicit Event( const uint128_t& type );

#ifdef BOOST_NO_CXX11_RVALUE_REFERENCES
    /** Copy ctor @internal */
    ZEQ_API Event( const Event& rhs )
        : _impl(0)
    {
        copy( rhs );
    }

    /** Assignment operator @internal */
    ZEQ_API Event& operator=( const Event& rhs )
    {
        copy( rhs );
        return *this;
    }
#else
    /** Move ctor @internal */
    ZEQ_API Event( Event&& rhs );
#endif

    ZEQ_API ~Event();

    /** @return the type of this event */
    ZEQ_API const uint128_t& getType() const;

    /** @internal @return the size in bytes of the serialized data */
    ZEQ_API size_t getSize() const;

    /** @internal @return the serialized data */
    ZEQ_API const void* getData() const;

    /** @internal @return serialization specific implementation */
    ZEQ_API flatbuffers::FlatBufferBuilder& getFBB();

    /** @internal @return serialization specific implementation */
    ZEQ_API flatbuffers::Parser& getParser();

private:
    Event( const Event& ) = delete;
    Event& operator=( const Event& ) = delete;

    friend class detail::Subscriber;
    void setData( const void* data, const size_t size );
    void copy( const Event& rhs );

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    Event& operator=( Event&& rhs );
#endif

    detail::Event* _impl;
};

}

#endif
