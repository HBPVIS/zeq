
/* Copyright (c) 2014, Human Brain Project
 *                     Daniel Nachbaur <daniel.nachbaur@epfl.ch>
 */

#ifndef ZEQ_DETAIL_SERIALIZATION_H
#define ZEQ_DETAIL_SERIALIZATION_H

#include <zeq/camera_generated.h>
#include <zeq/selections_generated.h>
#include <zeq/vocabulary.h>
#include "event.h"
#include <lunchbox/debug.h>

namespace zeq
{
namespace detail
{

template< typename T, typename Builder >
void buildVectorOnlyBuffer(
    zeq::Event& event,
    void (Builder::*adder)( flatbuffers::Offset< flatbuffers::Vector< T >>),
    const std::vector< T >& vector)
{
    flatbuffers::FlatBufferBuilder& fbb = event.getImpl()->fbb;
    Builder builder( fbb );
    (builder.*adder)( fbb.CreateVector( vector.data(), vector.size() ));
    fbb.Finish( builder.Finish( ));
}

#define BUILD_VECTOR_ONLY_BUFFER( event, type, attribute, vector ) \
    buildVectorOnlyBuffer( event, &type##Builder::add_##attribute, vector);

template< typename T, typename U >
std::vector< T > deserializeVector(
    const U* data,
    const flatbuffers::Vector< T > *(U::*getter)( ) const )
{
    const flatbuffers::Vector< T > *in = ( data->*getter )();
    std::vector< T > out( in->Length( ));
    for( size_t i = 0; i < in->Length(); ++i )
        out[i] = in->Get( i );
    return out;
}

template< typename T, typename U >
std::vector< T > deserializeVector(
    const zeq::Event& event,
    const flatbuffers::Vector< T > *(U::*getter)( ) const )
{
    const U* data = flatbuffers::GetRoot< U >( event.getData( ));
    return deserializeVector( data, getter );
}


zeq::Event serializeCamera( const std::vector< float >& matrix )
{
    if( matrix.size() != 16 )
        return zeq::Event( vocabulary::EVENT_INVALID );

    zeq::Event event( vocabulary::EVENT_CAMERA );
    BUILD_VECTOR_ONLY_BUFFER( event, Camera, matrix, matrix );
    return event;
}

std::vector< float > deserializeCamera( const zeq::Event& event )
{
    auto data = flatbuffers::GetRoot< Camera >( event.getData( ));
    LBASSERT( data->matrix()->Length() == 16 );
    return deserializeVector(data, &Camera::matrix);
}


zeq::Event serializeIDSelection( const std::vector< unsigned int >& ids )
{
    zeq::Event event( vocabulary::EVENT_ID_SELECTION );
    BUILD_VECTOR_ONLY_BUFFER( event, IDSelection, ids, ids );
    return event;
}

std::vector< unsigned int > deserializeIDSelection( const zeq::Event& event )
{
    return deserializeVector(event, &IDSelection::ids);
}

zeq::Event serializeToggleIDsRequest( const std::vector< unsigned int >& ids )
{
    zeq::Event event( vocabulary::EVENT_TOGGLE_IDS_REQUEST );
    BUILD_VECTOR_ONLY_BUFFER( event, ToggleIDsRequest, ids, ids );
    return event;
}

std::vector< unsigned int > deserializeToggleIDsRequest(
    const zeq::Event& event )
{
    return deserializeVector(event, &ToggleIDsRequest::ids);
}

}
}

#endif
