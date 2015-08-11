# Copyright (c) HBP 2014-2015 Juan Hernando <jhernando@fi.upm.es>
#                             Daniel Nachbaur <daniel.nachbaur@epfl.ch>
# All rights reserved. Do not distribute without further notice.

flatbuffers_generate_c_headers(HBP_FBS
  detail/camera.fbs
  detail/frame.fbs
  detail/imageJPEG.fbs
  detail/lookupTable1D.fbs
  detail/selections.fbs
  detail/animation.fbs
)

set(ZEQHBP_PUBLIC_HEADERS vocabulary.h ${HBP_FBS_ZEQ_OUTPUTS})
set(ZEQHBP_SOURCES vocabulary.cpp)
set(ZEQHBP_LINK_LIBRARIES ${LUNCHBOX_LIBRARIES} ${LIBZMQ_LIBRARIES} zeq)
if(MSVC)
  list(APPEND ZEQHBP_LINK_LIBRARIES Ws2_32)
endif()

list(APPEND CPPCHECK_EXTRA_ARGS
  --suppress=*:${CMAKE_CURRENT_BINARY_DIR}/*_generated.h)

set(ZEQHBP_INCLUDE_NAME zeq/hbp)
set(ZEQHBP_NAMESPACE zeqhbp)
add_definitions(-DZEQ_SHARED)
common_library(zeqHBP)
