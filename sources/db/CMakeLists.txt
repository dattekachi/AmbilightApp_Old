# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/db)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/db)

FILE ( GLOB DB_SOURCES "${CURRENT_HEADER_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" )

add_library(database
	${DB_SOURCES}
)

target_link_libraries(database
	ambilightapp-base
	ambilightapp-utils
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Sql
)

if (USE_STATIC_QT_PLUGINS)
	target_link_libraries(database ${STATIC_QT_PLUGINS_LIBS})
endif()

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
    target_precompile_headers(database REUSE_FROM precompiled_ambilightapp_headers)
endif()
