# generate windows .rc file for this binary
if (WIN32)
	include(${CMAKE_SOURCE_DIR}/cmake/win/win_rc.cmake)
	generate_win_rc_file(ambilightapp)
endif()

if (NOT ENABLE_POWER_MANAGEMENT)	
elseif (WIN32)
	set(ambilightapp_POWER_MNG "SuspendHandlerWindows.cpp;SuspendHandlerWindows.h")
elseif (APPLE)
	set(ambilightapp_POWER_MNG "SuspendHandlerMacOS.mm;SuspendHandlerMacOS.h")
	find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
elseif (Qt${Qt_VERSION}DBus_FOUND)
	set(ambilightapp_POWER_MNG "SuspendHandlerLinux.cpp;SuspendHandlerLinux.h")
	set(ambilightapp_POWER_MNG_DBUS "Qt${Qt_VERSION}::DBus")
endif()

add_executable(ambilightapp
	AmbilightAppDaemon.h
	systray.h
	AmbilightAppDaemon.cpp
	systray.cpp
	main.cpp
	${ambilightapp_WIN_RC_PATH}
	${ambilightapp_POWER_MNG}
)

if (UNIX AND NOT APPLE AND Qt${Qt_VERSION}DBus_FOUND)
	target_compile_definitions(ambilightapp PUBLIC AMBILIGHTAPP_HAVE_DBUS)
endif()

# promote ambilightapp as GUI app
if (WIN32)
	target_link_options(ambilightapp PUBLIC /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
endif()

target_link_libraries(ambilightapp
	commandline
	ambilightapp-base
	effectengine
	jsonserver
	flatbufserver
	webserver
	ssdp
	database
	resources
	Qt${Qt_VERSION}::Widgets
	${ambilightapp_POWER_MNG_DBUS}
	${APPKIT_FRAMEWORK}
	${TURBOJPEG_LINK_LIBRARIES}
)

if (USE_STATIC_QT_PLUGINS)
	target_link_libraries(ambilightapp ${STATIC_QT_PLUGINS_LIBS})
endif()

if(ENABLE_XZ)
	if (LIBLZMA_FOUND)
		target_link_libraries(ambilightapp LibLZMA::LibLZMA)
	else()
		target_link_libraries(ambilightapp liblzma)
	endif()
endif()

if (ENABLE_PROTOBUF)
	target_link_libraries(ambilightapp proto-nano-server)
endif ()

if (ENABLE_MQTT)
	target_link_libraries(ambilightapp mqtt)
endif ()

if (ENABLE_BONJOUR)
	target_link_libraries(ambilightapp bonjour)
endif ()

if (ENABLE_CEC)
	target_link_libraries(ambilightapp cechandler)
endif ()
		
if (ENABLE_V4L2)
	target_link_libraries(ambilightapp v4l2-grabber)
endif ()

if (ENABLE_MF)
	target_link_libraries(ambilightapp MF-grabber)
endif ()

if (ENABLE_DX)
	target_link_libraries(ambilightapp DX-grabber)
endif ()

if (ENABLE_X11)
	target_link_libraries(ambilightapp X11-grabber "${CMAKE_DL_LIBS}")
endif ()

if (ENABLE_FRAMEBUFFER)
	target_link_libraries(ambilightapp framebuffer-grabber "${CMAKE_DL_LIBS}")
endif ()

if (ENABLE_PIPEWIRE)
	target_link_libraries(ambilightapp Pipewire-grabber "${CMAKE_DL_LIBS}")
endif ()

if (ENABLE_MAC_SYSTEM)
	target_link_libraries(ambilightapp
		MACOS-grabber
		"-framework CoreGraphics"
		"-framework Foundation")
endif()

if (ENABLE_AVF)
	target_link_libraries(ambilightapp 
		AVF-grabber
		"-framework CoreGraphics"
		"-framework AVFoundation"
		"-framework Foundation"
		"-framework CoreMedia"
		"-framework CoreVideo"
		"-framework Accelerate"
		"-framework IOKit")
endif ()

if (ENABLE_SOUNDCAPWINDOWS)
	target_link_libraries(ambilightapp sound-capture-windows)
endif ()

if (ENABLE_SOUNDCAPLINUX)
	target_link_libraries(ambilightapp sound-capture-linux)
endif ()

if (ENABLE_SOUNDCAPMACOS)
	target_link_libraries(ambilightapp sound-capture-macos)
endif ()


if(APPLE)
	install ( TARGETS ambilightapp DESTINATION "share/.." COMPONENT "AmbilightAPP" )	
elseif(NOT WIN32)
	install ( TARGETS ambilightapp DESTINATION "share/ambilightapp/bin" COMPONENT "AmbilightAPP" )
	install ( DIRECTORY ${CMAKE_SOURCE_DIR}/cmake/debian/service DESTINATION "share/ambilightapp/" COMPONENT "AmbilightAPP" )
	install ( FILES ${CMAKE_SOURCE_DIR}/resources/icons/ambilightapp-icon-32px.png DESTINATION "share/ambilightapp/icons" COMPONENT "AmbilightAPP" )

	# Desktop file for ambilightapp
	install ( FILES ${CMAKE_SOURCE_DIR}/cmake/desktop/ambilightapp_128.png DESTINATION "share/ambilightapp/desktop" COMPONENT "AmbilightAPP" )
	install ( FILES ${CMAKE_SOURCE_DIR}/cmake/desktop/ambilightapp.desktop DESTINATION "share/ambilightapp/desktop" COMPONENT "AmbilightAPP" )
else()
	install ( TARGETS ambilightapp DESTINATION "bin" COMPONENT "AmbilightAPP" )

	#set( CMAKE_INSTALL_UCRT_LIBRARIES TRUE )
	#set( CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE )
	include( InstallRequiredSystemLibraries )
endif()

if(CMAKE_HOST_UNIX AND NOT APPLE)
	install( CODE "EXECUTE_PROCESS(COMMAND ln -sf \"../share/ambilightapp/bin/ambilightapp\" \"${CMAKE_BINARY_DIR}/symlink_ambilightapp\" )" COMPONENT "AmbilightAPP" )
	install( FILES ${CMAKE_BINARY_DIR}/symlink_ambilightapp DESTINATION "bin" RENAME ambilightapp COMPONENT "AmbilightAPP" )
	install( CODE "FILE (REMOVE ${CMAKE_BINARY_DIR}/symlink_ambilightapp )" COMPONENT "AmbilightAPP" )
endif()

# Copy dependencies (not for OSX)
include(${CMAKE_SOURCE_DIR}/cmake/Dependencies.cmake)

if(WIN32)
		if (Qt_VERSION EQUAL 5)
			get_target_property(QT_QMAKE_EXECUTABLE ${Qt5Core_QMAKE_EXECUTABLE} IMPORTED_LOCATION)
			get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
			find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")
		else()
			get_filename_component(My_Qt6Core_EXECUTABLE_DIR ${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS} ABSOLUTE)
			find_program(WINDEPLOYQT_EXECUTABLE windeployqt PATHS "${My_Qt6Core_EXECUTABLE_DIR}" NO_DEFAULT_PATH)
			if (NOT WINDEPLOYQT_EXECUTABLE)
				find_program(WINDEPLOYQT_EXECUTABLE windeployqt)
			endif()
		endif()
		
		if (WINDEPLOYQT_EXECUTABLE AND NOT CMAKE_GITHUB_ACTION)
			set(WINDEPLOYQT_PARAMS_RUNTIME --verbose 0 --no-compiler-runtime --no-opengl-sw --no-system-d3d-compiler)
			message(STATUS "Found windeployqt: ${WINDEPLOYQT_EXECUTABLE} PATH_HINT:${My_Qt6Core_EXECUTABLE_DIR}${QT_BIN_DIR}")
			add_custom_command(TARGET ambilightapp POST_BUILD COMMAND ${WINDEPLOYQT_EXECUTABLE} ${WINDEPLOYQT_PARAMS_RUNTIME} "$<TARGET_FILE:ambilightapp>")			
		endif()
endif()		

if (APPLE)
	DeployApple("ambilightapp")
elseif (NOT WIN32) # Unix
	DeployUnix("ambilightapp")
elseif(WIN32) # Windows
	DeployWindows("ambilightapp")
endif ()

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
    target_precompile_headers(ambilightapp REUSE_FROM precompiled_ambilightapp_headers)
endif()
