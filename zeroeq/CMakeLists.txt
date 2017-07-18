# Copyright (c) HBP 2014-2017 Daniel.Nachbaur@epfl.ch

list(APPEND CPPCHECK_EXTRA_ARGS
  --suppress=*:${CMAKE_CURRENT_BINARY_DIR}/*_generated.h)

set(ZEROEQ_PUBLIC_HEADERS
  connection/broker.h
  connection/service.h
  log.h
  monitor.h
  publisher.h
  receiver.h
  sender.h
  subscriber.h
  types.h
  uri.h)

set(ZEROEQ_HEADERS
  detail/application.h
  detail/broker.h
  detail/constants.h
  detail/context.h
  detail/port.h
  detail/sender.h
  detail/socket.h)

set(ZEROEQ_SOURCES
  connection/broker.cpp
  connection/service.cpp
  detail/context.cpp
  detail/port.cpp
  detail/sender.cpp
  monitor.cpp
  publisher.cpp
  receiver.cpp
  subscriber.cpp
  uri.cpp)

set(ZEROEQ_LINK_LIBRARIES PUBLIC Servus
                          PRIVATE ${CMAKE_THREAD_LIBS_INIT} ${libzmq_LIBRARIES})
if(MSVC)
  list(APPEND ZEROEQ_LINK_LIBRARIES Ws2_32)
endif()

common_library(ZeroEQ)

if(CPPNETLIB_FOUND)
  add_subdirectory(http)
endif()
