cmake_minimum_required(VERSION 3.11)

project(lasershark_hostapp)

add_definitions(-DNOMINMAX)
add_definitions(-D_USE_MATH_DEFINES)
add_definitions(-Wall)

message("Build type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/Modules;${CMAKE_MODULE_PATH}")

if(CYGWIN)
	# add .lib for find libjack64.lib (FindJack.cmake)
	set(CMAKE_FIND_LIBRARY_SUFFIXES ".dll.a" ".a" ".lib")
endif()

if(MINGW)
add_library(unbuffered STATIC mingw_unbuffered.c)
set(COMMON_LIBRARIES unbuffered)
endif()

# libusb
find_package(Libusb)
include_directories(${LIBUSB_INCLUDE_DIR})

# jack
find_package(JACK)
include_directories(${JACK_INCLUDE_DIRS})

# laserdock_jack
add_executable(laserdock_jack)
target_sources(laserdock_jack PRIVATE laserdock_jack.c lasersharklib/lasershark_lib.c lasersharklib/lasershark_lib.h)
target_link_libraries(laserdock_jack PRIVATE ${JACK_LIBRARIES} ${LIBUSB_LIBRARIES} ${COMMON_LIBRARIES})
install(TARGETS laserdock_jack RUNTIME DESTINATION bin)

string(REGEX REPLACE .lib$ .dll LIBUSB_DLL ${LIBUSB_LIBRARIES})
message("LIBUSB_DLL=${LIBUSB_DLL}")
message("LIBUSB_DLL=${LIBUSB_LIBRARIES}")
install(FILES ${LIBUSB_DLL} DESTINATION bin)
if(MSVC)
    add_custom_command(TARGET laserdock_jack POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${LIBUSB_DLL} ${CMAKE_CURRENT_BINARY_DIR})
endif()

# lasershark_jack
if(NOT (MSVC OR MINGW))
    add_executable(lasershark_jack)
    target_sources(lasershark_jack PRIVATE lasershark_jack.c lasersharklib/lasershark_lib.c lasersharklib/lasershark_lib.h)
    target_link_libraries(lasershark_jack PRIVATE ${JACK_LIBRARIES} ${LIBUSB_LIBRARIES} ${COMMON_LIBRARIES})
endif()

# lasershark_stdin_circlemaker
add_executable(lasershark_stdin_circlemaker)
target_sources(lasershark_stdin_circlemaker PRIVATE lasershark_stdin_circlemaker.c)
if(NOT MSVC)
	target_link_libraries(lasershark_stdin_circlemaker PRIVATE m ${COMMON_LIBRARIES})
endif()

# lasershark_stdin
# TODO support MSVC and MINGW
if(NOT (MSVC OR MINGW))
    add_executable(lasershark_stdin)
    target_sources(lasershark_stdin PRIVATE lasershark_stdin.c lasersharklib/lasershark_lib.c lasersharklib/lasershark_lib.h)
    target_link_libraries(lasershark_stdin PRIVATE ${JACK_LIBRARIES} ${LIBUSB_LIBRARIES} ${COMMON_LIBRARIES})
endif()

# lasershark_twostep
if(NOT (MSVC OR MINGW OR CYGWIN))
	add_executable(lasershark_twostep)
	target_sources(lasershark_twostep PRIVATE lasershark_twostep.c lasershark_uart_bridge_lib.c ls_ub_twostep_lib.c twostep_host_lib.c twostep_common_lib.c)
	target_link_libraries(lasershark_twostep PRIVATE ${LIBUSB_LIBRARIES} ${COMMON_LIBRARIES})
endif()
