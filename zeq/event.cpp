
/* Copyright (c) 2014, Human Brain Project
 *                     Daniel Nachbaur <daniel.nachbaur@epfl.ch>
 */

#include "event.h"
#include "detail/event.h"

namespace zeq
{

Event::Event( const uint64_t type )
    : _impl( new detail::Event( type ))
{}

Event::Event( Event&& rhs )
    : _impl( rhs._impl )
{
    rhs._impl = 0;
}

Event::~Event()
{
    delete _impl;
}

uint64_t Event::getType() const
{
    return _impl->type;
}

flatbuffers::FlatBufferBuilder &Event::getFlatBuffer()
{
    return _impl->fbb;
}

size_t Event::getSize() const
{
    return _impl->getSize();
}

const void* Event::getData() const
{
    return _impl->getData();
}

void Event::setData( const void* data, const size_t size )
{
    _impl->setData( data, size );
}

detail::Event* Event::getImpl()
{
    return _impl;
}

}
