# Copyright (c) HBP 2014-2015 Daniel.Nachbaur@epfl.ch

flatbuffers_generate_c_headers(ZEROEQ_FBS
  detail/echo.fbs
  detail/heartbeat.fbs
  detail/request.fbs
  detail/vocabulary.fbs)

list(APPEND CPPCHECK_EXTRA_ARGS
  --suppress=*:${CMAKE_CURRENT_BINARY_DIR}/*_generated.h)

set(ZEROEQ_PUBLIC_HEADERS
  ${ZEROEQ_FBS_ZEROEQ_OUTPUTS}
  connection/broker.h
  connection/service.h
  event.h
  eventDescriptor.h
  log.h
  publisher.h
  receiver.h
  subscriber.h
  types.h
  uri.h
  vocabulary.h)

set(ZEROEQ_HEADERS
  detail/broker.h
  detail/constants.h
  detail/event.h
  detail/eventDescriptor.h
  detail/port.h
  detail/sender.h
  detail/socket.h
  detail/vocabulary.h)

set(ZEROEQ_SOURCES
  connection/broker.cpp
  connection/service.cpp
  detail/port.cpp
  detail/sender.cpp
  detail/vocabulary.cpp
  event.cpp
  eventDescriptor.cpp
  publisher.cpp
  receiver.cpp
  subscriber.cpp
  uri.cpp
  vocabulary.cpp)

set(ZEROEQ_LINK_LIBRARIES PUBLIC Servus
                          PRIVATE ${CMAKE_THREAD_LIBS_INIT} ${LIBZMQ_LIBRARIES}
                                  flatbuffers)
if(MSVC)
  list(APPEND ZEROEQ_LINK_LIBRARIES Ws2_32)
endif()
if(HTTPXX_FOUND)
  list(APPEND ZEROEQ_PUBLIC_HEADERS http/server.h)
  list(APPEND ZEROEQ_SOURCES http/server.cpp)
  list(APPEND ZEROEQ_LINK_LIBRARIES ${HTTPXX_LIBRARIES})
endif()

common_library(ZeroEQ)
add_subdirectory(hbp)
