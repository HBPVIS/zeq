# Copyright (c) HBP 2014-2016 Daniel.Nachbaur@epfl.ch

set(ZEROEQHTTP_PUBLIC_HEADERS server.h)
set(ZEROEQHTTP_HEADERS jsoncpp/json/json.h jsoncpp/json/json-forwards.h)
set(ZEROEQHTTP_SOURCES server.cpp jsoncpp/jsoncpp.cpp)
set(ZEROEQHTTP_LINK_LIBRARIES PUBLIC ZeroEQ
                              PRIVATE ${HTTPXX_LIBRARIES} ${LIBZMQ_LIBRARIES})

if(NOT MSVC)
  set_source_files_properties(jsoncpp/jsoncpp.cpp PROPERTIES COMPILE_FLAGS
    -Wno-shadow)
endif()

set(ZEROEQHTTP_INCLUDE_NAME zeroeq/http)
set(ZEROEQHTTP_NAMESPACE zeroeqhttp)
set(ZEROEQHTTP_OMIT_LIBRARY_HEADER ON)
common_library(ZeroEQHTTP)
