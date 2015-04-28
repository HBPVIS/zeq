
/* Copyright (c) 2014-2015, Human Brain Project
 *                          Daniel Nachbaur <daniel.nachbaur@epfl.ch>
 */

#include "event.h"

#include "../eventDescriptor.h"
#include "../vocabulary.h"

#include <zeq/echo_generated.h>
#include <zeq/request_generated.h>
#include <zeq/vocabulary_generated.h>

#include <lunchbox/debug.h>
#include <lunchbox/stdExt.h>

#include <boost/foreach.hpp>

namespace zeq
{
namespace vocabulary
{
namespace detail
{
namespace
{
stde::hash_map< uint128_t, std::string >& getRegistry()
{
    static stde::hash_map< uint128_t, std::string > _eventRegistry;
    return _eventRegistry;
}
}

Event serializeVocabulary( const EventDescriptors& vocabulary )
{
    ::zeq::Event event( EVENT_VOCABULARY );
    flatbuffers::FlatBufferBuilder& fbb = event.getFBB();

    std::vector< flatbuffers::Offset<flatbuffers::String> > restNames;
    std::vector< uint64_t > eventTypeLows;
    std::vector< uint64_t > eventTypeHighs;
    std::vector< flatbuffers::Offset<flatbuffers::String> > schemas;
    std::vector< uint8_t > eventDirections;

  for( EventDescriptors::const_iterator eventDescriptor=vocabulary.begin();
      eventDescriptor!=vocabulary.end(); ++eventDescriptor)
    {
        restNames.push_back( fbb.CreateString( eventDescriptor->getRestName( )));
        eventTypeHighs.push_back( eventDescriptor->getEventType().high( ));
        eventTypeLows.push_back( eventDescriptor->getEventType().low( ));
        schemas.push_back( fbb.CreateString( eventDescriptor->getSchema( )));
        eventDirections.push_back( uint8_t( eventDescriptor->getEventDirection()));
    }

    const auto& restNamesForZeq = fbb.CreateVector( &restNames[0],
                                                    restNames.size( ));
    const auto& eventTypeHighsForZeq = fbb.CreateVector( &eventTypeHighs[0],
                                                        eventTypeHighs.size( ));
    const auto& eventTypeLowsForZeq = fbb.CreateVector( &eventTypeLows[0],
                                                         eventTypeLows.size( ));
    const auto& schemasForZeq = fbb.CreateVector( &schemas[0], schemas.size( ));
    const auto& eventDirectionsForZeq = fbb.CreateVector( &eventDirections[0],
                                        eventDirections.size() );

    VocabularyBuilder builder( fbb );
    builder.add_restNames( restNamesForZeq );
    builder.add_eventHighs( eventTypeHighsForZeq );
    builder.add_eventLows( eventTypeLowsForZeq );
    builder.add_schemas( schemasForZeq );
    builder.add_eventDirections( eventDirectionsForZeq );

    fbb.Finish( builder.Finish() );
    return event;
}

EventDescriptors deserializeVocabulary( const Event& event )
{
    const auto& data = GetVocabulary( event.getData() );

    EventDescriptors vocabulary;
    vocabulary.reserve( data->restNames()->Length() );

    for( flatbuffers::uoffset_t i = 0; i < data->restNames()->Length(); ++i )
    {
        const uint128_t eventType( data->eventHighs()->Get(i),
                                   data->eventLows()->Get(i));
        EventDescriptor restZeqEvent( data->restNames()->Get(i)->c_str(),
                                      eventType,
                                      data->schemas()->Get(i)->c_str(),
                                      zeq::EventDirection(data->eventDirections()->Get(i) ) );
        vocabulary.push_back( std::move( restZeqEvent ));
    }

    return vocabulary;
}

::zeq::Event serializeRequest( const uint128_t& eventType )
{
    ::zeq::Event event( ::zeq::vocabulary::EVENT_REQUEST );
    flatbuffers::FlatBufferBuilder& fbb = event.getFBB();

    RequestBuilder builder( fbb );
    builder.add_eventHigh( eventType.high());
    builder.add_eventLow( eventType.low());

    fbb.Finish( builder.Finish( ));
    return event;
}

uint128_t deserializeRequest( const ::zeq::Event& event )
{
    auto data = GetRequest( event.getData( ));
    return uint128_t( data->eventHigh(), data->eventLow());
}

zeq::Event serializeEcho( const std::string& msg )
{
    zeq::Event event( ::zeq::vocabulary::EVENT_ECHO );

    flatbuffers::FlatBufferBuilder& fbb = event.getFBB();
    EchoBuilder builder( fbb );
    builder.add_message( fbb.CreateString( msg ));
    fbb.Finish( builder.Finish( ));
    return event;
}

std::string deserializeEcho( const zeq::Event& event )
{
    auto data = GetEcho( event.getData( ));
    return data->message()->c_str();
}

zeq::Event serializeJSON( const uint128_t& type, const std::string& json )
{
    zeq::Event event( type );
    flatbuffers::Parser& parser = event.getParser();
    if( !parser.Parse( json.c_str( )))
        LBTHROW( std::runtime_error( parser.error_ ));
    return event;
}

std::string deserializeJSON( const zeq::Event& event )
{
    std::string json;
    flatbuffers::GeneratorOptions opts;
    opts.base64_byte_array = true;
    opts.strict_json = true;
    GenerateText( event.getParser(), event.getData(), opts, &json );
    return json;
}

void registerEvent( const uint128_t& type, const std::string& schema )
{
    getRegistry()[type] = schema;
}

const std::string& getSchema( const uint128_t& type )
{
    return getRegistry()[type];
}

}
}
}
