# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/base)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/base)

include_directories(
	${CMAKE_CURRENT_BINARY_DIR}/../../sources/flatbufserver
)

FILE ( GLOB AmbilightAPP_SOURCES "${CURRENT_HEADER_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" )

SET(AmbilightAPP_RESOURCES ${CURRENT_SOURCE_DIR}/resource.qrc)

add_library(ambilightapp-base
	${AmbilightAPP_SOURCES}
	${AmbilightAPP_RESOURCES}
)

target_link_libraries(ambilightapp-base
	blackborder
	ambilightapp-utils
	flatbufserver
	flatbuffers
	leddevice
	effectengine
	database
	${QT_LIBRARIES}
)

if(ENABLE_BOBLIGHT)
	target_link_libraries(ambilightapp-base boblightserver)
endif()

if (ENABLE_BONJOUR)
	target_link_libraries(ambilightapp-base bonjour)
endif ()

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
    target_precompile_headers(ambilightapp-base REUSE_FROM precompiled_ambilightapp_headers)
endif()
