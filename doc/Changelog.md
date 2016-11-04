# Changelog {#Changelog}

# git master

* [184](https://github.com/HBPVIS/ZeroEQ/pull/184):
  Use cppnetlib to implement HTTP server to support multiple clients
* [181](https://github.com/HBPVIS/ZeroEQ/pull/181):
  Specification for registry and schema API using REST
* [179](https://github.com/HBPVIS/ZeroEQ/pull/179):
  Fix http server blocking issues with libzmq 4.1.4

# Release 0.6 (30-06-2016)

* [170](https://github.com/HBPVIS/ZeroEQ/pull/170):
  Remove FlatBuffers dependency and support
* [169](https://github.com/HBPVIS/ZeroEQ/pull/169):
  Fix #157: http server may hang in receive() when remote connection is closed
  abruptly
* [167](https://github.com/HBPVIS/ZeroEQ/pull/167):
  Move HBP vocabulary to Lexis
* [166](https://github.com/HBPVIS/ZeroEQ/pull/166):
  Implement event-based communication as per
  [152](https://github.com/HBPVIS/ZeroEQ/pull/152)
* [161](https://github.com/HBPVIS/ZeroEQ/pull/161):
  Move progressMonitor tool to Lexis
* [145](https://github.com/HBPVIS/ZeroEQ/pull/145):
  * The zeroeq::Event class is renamed to zeroeq::FBEvent
  * zeroeq::FBEvent is derived from servus::Serializable
  * Removed deprecated zeroeq::FBEvent functions from pub/sub
  * Removed EVENT_EXIT
  * Removed RESTBridge related code
  * FlatBuffers is an optional dependency
* [143](https://github.com/HBPVIS/ZeroEQ/pull/143):
  Rename zeq to ZeroEQ in the whole project
* [141](https://github.com/HBPVIS/ZeroEQ/pull/141):
  * Removed deprecated servus::URI based constructors from the pub/sub API
  * The returned URIs from pub/sub and http server is zeq::URI
  * The zeq::URI construction from servus::URI is explicit
* [140](https://github.com/HBPVIS/ZeroEQ/pull/140):
  Added getSocketDescriptor() to http::server
* [138](https://github.com/HBPVIS/ZeroEQ/pull/138):
  Command line progress monitor

# Release 0.5 (10-03-2016)

* [132](https://github.com/HBPVIS/ZeroEQ/pull/132):
  Deprecate FlatBuffers based API in publisher/subscriber
* [129](https://github.com/HBPVIS/ZeroEQ/pull/129):
  Added GET and PUT notifications to http server
* [121](https://github.com/HBPVIS/ZeroEQ/pull/121):
  zeq::URI allows to use ":port" only
* [116](https://github.com/HBPVIS/zeq/issues/115):
  Add zeq::http::Server
* [116](https://github.com/HBPVIS/ZeroEQ/pull/116):
  Generalize Zerobuf support into abstract servus::Serializable
* [109](https://github.com/HBPVIS/ZeroEQ/pull/109):
  Implement session filtering from Sessions.md RFC
  * Custom/user-defined schemas in URIs for publisher and subscriber are used
    as session name. Porting to new constructors with explicit session name
    is recommended if fixed session name is desired.
  * zeq::Publisher::getPort() removed, use
    zeq::Publisher::getURI().getPort() instead

# Release 0.4 (02-11-2015)

* [98](https://github.com/HBPVIS/ZeroEQ/pull/98):
  Disable zeroconf subscriptions to publishers in the same process
* [97](https://github.com/HBPVIS/ZeroEQ/pull/97):
  Added a small command line tool to send events parsed from a script file.
* [94](https://github.com/HBPVIS/ZeroEQ/pull/94):
  Added CellSetBinaryOp to HBP vocabulary
* [81](https://github.com/HBPVIS/ZeroEQ/pull/81):
  Implement (optional) ZeroBuf support

# Release 0.3 (07-07-2015)

* [75](https://github.com/HBPVIS/ZeroEQ/pull/75):
  Event for frame setting and animation
* [74](https://github.com/HBPVIS/ZeroEQ/pull/74):
  Fix exception with broken DNS and zeroconf
* [69](https://github.com/HBPVIS/ZeroEQ/pull/69):
  Replaced Lunchbox by Servus
* [68](https://github.com/HBPVIS/ZeroEQ/pull/68):
  OPT: Filter messages for registered handlers
* [64](https://github.com/HBPVIS/ZeroEQ/pull/64):
  Allow specification of network announce protocols in Publisher
* [63](https://github.com/HBPVIS/ZeroEQ/pull/63):
  Replace boost by C++11 std equivalents

# Release 0.2 (01-05-2015)

* zeq::connection::Broker and zeq::connection::Service for subscription from a
  remote publisher.
* Concurrently dispatch events for multiple zeq::Subscriber and
  zeq::connection::Broker using shared zeq::Receiver groups.

# Release 0.1 (07-01-2015)

* zeq::Publisher, zeq::Subscriber and zeq::Event for publish-subcribe event
  driven architectures.
