
/* Copyright (c) 2014, Human Brain Project
 *                     Daniel Nachbaur <daniel.nachbaur@epfl.ch>
 */

#ifndef ZEQ_EVENT_H
#define ZEQ_EVENT_H

#include <zeq/api.h>
#include <zeq/types.h>

namespace flatbuffers
{
    class FlatBufferBuilder;
}

namespace zeq
{
namespace detail { class Subscriber; class Event; }

/**
 * An event can notify other subscribers of a state change.
 *
 * Events are published via zeq::Publisher and received via zeq::Subscriber. The
 * format of the serialized data is specific to the serialization backend.
 */
class Event : public boost::noncopyable
{
public:
    /**
     * Construct a new event of the given type
     * @param type the desired event type
     */
    ZEQ_API explicit Event( const uint64_t type );

    /** Move ctor @internal */
    Event( Event&& rhs );

    ZEQ_API ~Event();

    /** @return the type of this event */
    ZEQ_API uint64_t getType() const;

    /** @return the flat buffer builder */
    ZEQ_API flatbuffers::FlatBufferBuilder& getFlatBuffer();

    /** @internal @return the size in bytes of the serialized data */
    ZEQ_API size_t getSize() const;

    /** @internal @return the serialized data */
    const void* getData() const;

    /** @internal @return serialization specific implementation */
    detail::Event* getImpl();

private:
    friend class detail::Subscriber;
    void setData( const void* data, const size_t size );

    Event& operator=( Event&& rhs );

    detail::Event* _impl;
};

}

#endif
