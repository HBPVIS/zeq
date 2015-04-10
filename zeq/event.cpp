
/* Copyright (c) 2014-2015, Human Brain Project
 *                          Daniel Nachbaur <daniel.nachbaur@epfl.ch>
 */

#include "event.h"
#include "detail/event.h"

namespace zeq
{

Event::Event( const uint128_t& type )
    : _impl( new detail::Event( type ))
{}

#ifdef SUPPORT_FOR_CPP11
Event::Event( Event&& rhs )
    : _impl( rhs._impl )
{
    rhs._impl = 0;
}
#endif

Event::~Event()
{
    delete _impl;
}

const uint128_t& Event::getType() const
{
    LBASSERT(_impl);
    return _impl->type;
}

size_t Event::getSize() const
{
    LBASSERT(_impl);
    return _impl->getSize();
}

const void* Event::getData() const
{
    LBASSERT(_impl);
    return _impl->getData();
}

void Event::setData( const void* data, const size_t size )
{
    LBASSERT(_impl);
    _impl->setData( data, size );
}

flatbuffers::FlatBufferBuilder& Event::getFBB()
{
    LBASSERT(_impl);
    return _impl->parser.builder_;
}

flatbuffers::Parser& Event::getParser()
{
    LBASSERT(_impl);
    return _impl->parser;
}

const flatbuffers::Parser& Event::getParser() const
{
    LBASSERT(_impl);
    return _impl->parser;
}

}
