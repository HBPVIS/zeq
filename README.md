
# Introduction

ZeroEQ is a cross-platform C++ library to publish and subscribe for events.
Applications communicate using ZeroMQ, discover each other automatically through
the integrated ZeroConf protocol or through explicit addressing using hostname
and port. A defined vocabulary defines semantics for the published events,
provided by [ZeroBuf](https://github.com/HBPVIS/ZeroBuf) or using a simple
Serializable interface. An optional http::Server provides a web service API in
C++ applications.

# Features

ZeroEQ provides the following major features:

* Publish events using zeroeq::Publisher
* Subscribe to events using zeroeq::Subscriber
* Web service GET and PUT events using zeroeq::http::Server using REST and JSON
* Asynchronous, reliable transport using ZMQ
* Automatic publisher discovery using Zeroconf
* Serialization of events using [ZeroBuf](https://github.com/HBPVIS/ZeroBuf)
* [List of RFCs](@ref rfcs)

# Example Use Cases

* [Interactive Supercomputing](https://www.youtube.com/watch?v=wATHwvRFGz0&t=1m36s):
  loose, robust and fast coupling of unrelated applications
* [Integration of C++ applications into Jupyter Notebooks](https://www.youtube.com/watch?v=pczckc9HSsA&t=14m30s):
  automatic python code generation to remote control C++ applications using the
  zeroeq::http::Server

# Building from Source

ZeroEQ is a cross-platform library, designed to run on any modern operating
system, including all Unix variants. It requires a C++11 compiler and uses CMake
to create a platform-specific build environment. The following platforms and
build environments are tested:

* Linux: Ubuntu 16.04, RHEL 6.8 (Makefile, Ninja)
* Mac OS X: 10.9 (Makefile, Ninja)

ZeroEQ requires the following external, pre-installed dependencies:

* ZeroMQ 4.0 or later
* Boost (tested with 1.54) for unit tests

Building from source is as simple as:

    git clone https://github.com/HBPVIS/ZeroEQ.git
    mkdir ZeroEQ/build
    cd ZeroEQ/build
    cmake -GNinja ..
    ninja
This work has been partially funded by the European Union Seventh Framework Program (FP7/2007­2013) under grant agreement no. 604102 (HBP).
