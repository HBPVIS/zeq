
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

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
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

void Event::copy( const Event& rhs )
{
    if( _impl == rhs._impl )
        return;

    delete _impl;
    _impl = new detail::Event( rhs.getType() );
}

const uint128_t& Event::getType() const
{
    return _impl->type;
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

flatbuffers::FlatBufferBuilder& Event::getFBB()
{
    return _impl->parser.builder_;
}

flatbuffers::Parser& Event::getParser()
{
    return _impl->parser;
}

}
