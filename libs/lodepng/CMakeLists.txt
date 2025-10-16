set(LODEPNG_INCLUDE_DIR ./)

set(PUBLIC_HEADERS
    ${LODEPNG_INCLUDE_DIR}/lodepng.h
)

source_group("Public API" FILES ${PUBLIC_HEADERS})

set(SERIALIB_SOURCES
    ${LODEPNG_INCLUDE_DIR}/lodepng.cpp
)

add_library(lodepng STATIC ${PUBLIC_HEADERS} ${SERIALIB_SOURCES})

target_include_directories(lodepng
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})