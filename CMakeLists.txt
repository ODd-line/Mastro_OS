cmake_minimum_required(VERSION 3.16)

set(WATCH_BOARD "waveshare_amoled_1_8_v2" CACHE STRING "Mastro OS hardware profile")
set_property(CACHE WATCH_BOARD PROPERTY STRINGS "waveshare_amoled_1_8_v2")

if(NOT WATCH_BOARD STREQUAL "waveshare_amoled_1_8_v2")
	message(FATAL_ERROR
		"Unsupported WATCH_BOARD='${WATCH_BOARD}'. See BOARD_SUPPORT.md for verified and planned ports.")
endif()

message(STATUS "Mastro OS board profile: ${WATCH_BOARD}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(watch_os)