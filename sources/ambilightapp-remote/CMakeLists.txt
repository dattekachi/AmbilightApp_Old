project(ambilightapp-remote)

# The following I do not understand completely...
# libQtCore.so uses some hardcoded library path inside which are incorrect after copying the file RPi file system
# Therefore, an extra path is needed on which to find the required libraries
IF ( EXISTS ${CMAKE_FIND_ROOT_PATH}/lib/arm-linux-gnueabihf )
	LINK_DIRECTORIES(${LINK_DIRECTORIES} ${CMAKE_FIND_ROOT_PATH}/lib/arm-linux-gnueabihf)
ENDIF()

set(ambilightapp-remote_HEADERS
	JsonConnection.h)

set(ambilightapp-remote_SOURCES
	ambilightapp-remote.cpp
	JsonConnection.cpp)

# generate windows .rc file for this binary
if (WIN32)
	include(${CMAKE_SOURCE_DIR}/cmake/win/win_rc.cmake)
	generate_win_rc_file(${PROJECT_NAME})
endif()

add_executable(${PROJECT_NAME}
	${ambilightapp-remote_HEADERS}
	${ambilightapp-remote_SOURCES}
	${${PROJECT_NAME}_WIN_RC_PATH}
)

if(NOT WIN32)
	set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS_RELEASE -s)
endif()

if (USE_STATIC_QT_PLUGINS)
	target_link_libraries(${PROJECT_NAME} ${STATIC_QT_PLUGINS_LIBS})
endif()

target_link_libraries(${PROJECT_NAME}
	effectengine
	commandline
	ambilightapp-utils
	ssdp
	Qt${Qt_VERSION}::Gui
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Network
	${TURBOJPEG_LINK_LIBRARIES}
)

if(NOT WIN32)
	install ( TARGETS ${PROJECT_NAME} DESTINATION "share/ambilightapp/bin" COMPONENT "AmbilightAPP_remote" )
else()
	install ( TARGETS ${PROJECT_NAME} DESTINATION "bin" COMPONENT "AmbilightAPP_remote" )
endif()

if(CMAKE_HOST_UNIX)
	install(CODE "EXECUTE_PROCESS(COMMAND ln -sf \"../share/ambilightapp/bin/${PROJECT_NAME}\" \"${CMAKE_BINARY_DIR}/symlink_${PROJECT_NAME}\" )" COMPONENT "AmbilightAPP_remote" )
	install(FILES "${CMAKE_BINARY_DIR}/symlink_${PROJECT_NAME}" DESTINATION "bin" RENAME "${PROJECT_NAME}" COMPONENT "AmbilightAPP_remote" )
	install(CODE "FILE (REMOVE ${CMAKE_BINARY_DIR}/symlink_${PROJECT_NAME} )" COMPONENT "AmbilightAPP_remote" )
endif(CMAKE_HOST_UNIX)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
    target_precompile_headers(${PROJECT_NAME} REUSE_FROM precompiled_ambilightapp_headers)
endif()
