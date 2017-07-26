
/* Copyright (c) 2014-2017, Human Brain Project
 *                          Daniel Nachbaur <daniel.nachbaur@epfl.ch>
 *                          Juan Hernando <jhernando@fi.upm.es>
 */

#ifndef ZEROEQ_TYPES_H
#define ZEROEQ_TYPES_H

#include <functional>
#include <memory>
#include <servus/types.h>
#include <servus/uint128_t.h>
#include <zeroeq/defines.h>

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h> // SOCKET
#endif

/**
 * Publish-subscribe classes for typed events.
 *
 * A Publisher opens a listening port on the network, and publishes an Event on
 * this port. It announces its session for automatic discovery.
 *
 * A Subscriber either explicitely subscribes to the publisher port, or uses
 * automatic discovery to find publishers using the same session. Automatic
 * discovery is implemented using zeroconf networking (avahi or Apple Bonjour).
 *
 * The connection::Broker and connection::Service may be used to introduce a
 * subscriber to a remote, not zeroconf visible, publisher.
 *
 * An Event contains a strongly type, semantically defined message. Applications
 * or groups of applications can define their own vocabulary.
 */
namespace zeroeq
{
using servus::uint128_t;
class Monitor;
class Publisher;
class Sender;
class Subscriber;
class URI;

using URIs = std::vector<URI>; //!< A vector of URIs

/** Callback for receival of subscribed event w/o payload. */
using EventFunc = std::function<void()>;

/** Callback for receival of subscribed event w/ payload. */
using EventPayloadFunc = std::function<void(const void*, size_t)>;

#ifdef WIN32
typedef SOCKET SocketDescriptor;
#else
typedef int SocketDescriptor;
#endif

/** Constant defining 'wait forever' in methods with wait parameters. */
// Attn: identical to Win32 INFINITE!
static const uint32_t TIMEOUT_INDEFINITE = 0xffffffffu;

using servus::make_uint128;

static const std::string DEFAULT_SESSION = "__zeroeq";
static const std::string NULL_SESSION = "__null_session";

namespace detail
{
struct Socket;
}
}

#endif
