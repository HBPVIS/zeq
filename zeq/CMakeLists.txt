# Copyright (c) HBP 2014 Daniel.Nachbaur@epfl.ch
# All rights reserved. Do not distribute without further notice.

source_group(\\ FILES CMakeLists.txt)

FLATBUFFERS_GENERATE_C_HEADERS(SCHEMER detail/camera.fbs detail/selection.fbs)

set(ZEQ_PUBLIC_HEADERS ${COMMON_INCLUDES}
  event.h
  publisher.h
  subscriber.h
  types.h
  vocabulary.h)
set(ZEQ_HEADERS ${SCHEMER_OUTPUTS}
  detail/broker.h
  detail/event.h
  detail/serialization.h)
set(ZEQ_SOURCES ${COMMON_SOURCES}
  event.cpp
  publisher.cpp
  subscriber.cpp
  vocabulary.cpp)
set(ZEQ_LINK_LIBRARIES ${LUNCHBOX_LIBRARIES} ${LIBZMQ_LIBRARIES}
                       ${Boost_LIBRARIES})

common_library(zeq)
