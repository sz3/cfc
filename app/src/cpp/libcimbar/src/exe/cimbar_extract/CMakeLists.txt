cmake_minimum_required(VERSION 3.10)

project(cimbar_extract)

set (SOURCES
	cimbar_extract.cpp
)

add_executable (
	cimbar_extract
	${SOURCES}
)

target_link_libraries(cimbar_extract

	extractor

	${OPENCV_LIBS}
)

if(NOT MSVC)
	add_custom_command(
		TARGET cimbar_extract POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:cimbar_extract> cimbar_extract.dbg
		COMMAND ${CMAKE_STRIP} -g $<TARGET_FILE:cimbar_extract>
	)
endif()

install(
	TARGETS cimbar_extract
	DESTINATION bin
)

