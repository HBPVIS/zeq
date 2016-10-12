[TOC]

# Introduction

A cross-platform C++ library to publish and subscribe for events. Applications
communicate using ZeroMQ, discover each other automatically through the
integrated ZeroConf protocol or through explicit addressing using hostname and
port. A defined vocabulary defines semantics for the published events, provided
by [ZeroBuf](https://github.com/HBPVIS/ZeroBuf) or using a simple Serializable
interface. An optional http::Server provides a web service API in C++
applications.

## Features

ZeroEQ provides the following major features:

* Publish events using zeroeq::Publisher
* Subscribe to events using zeroeq::Subscriber
* Web service GET and PUT events using zeroeq::http::Server using REST and JSON
* Asynchronous, reliable transport using ZMQ
* Automatic publisher discovery using Zeroconf
* Serialization of events using [ZeroBuf](https://github.com/HBPVIS/ZeroBuf)
* [List of RFCs](@ref rfcs)

# Building

ZeroEQ is a cross-platform toolkit, designed to run on any modern operating
system, including all Unix variants. ZeroEQ uses CMake to create a
platform-specific build environment. The following platforms and build
environments are tested:

* Linux: Ubuntu 14.04, RHEL 6 using gcc 4.8
* Windows: 8 using Visual Studio 12
* Mac OS X: 10.9 and 10.10 using clang 6

ZeroEQ requires the following external, pre-installed dependencies:

* ZeroMQ 4.0 or later
* Boost (tested with 1.54) for unit tests

Building from source is as simple as:

    git clone https://github.com/HBPVIS/ZeroEQ.git
    mkdir ZeroEQ/build
    cd ZeroEQ/build
    cmake ..
    make

This work has been partially funded by the European Union Seventh Framework Program (FP7/2007­2013) under grant agreement no. 604102 (HBP).
