cmake_minimum_required(VERSION 3.10)

project(cimbar_recv2)

set (SOURCES
	recv2.cpp
)

add_executable (
	cimbar_recv2
	${SOURCES}
)

target_link_libraries(cimbar_recv2

	cimbar_js
	extractor

	${PLATFORM_GL}
	glfw
	${OPENCV_LIBS}
	opencv_videoio
)

if(MSVC)
	add_custom_command(
		TARGET cimbar_recv2 POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			$<TARGET_FILE:unofficial::angle::libEGL>
			$<TARGET_FILE_DIR:cimbar_recv2>
	)
else()
	add_custom_command(
		TARGET cimbar_recv2 POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:cimbar_recv2> cimbar_recv2.dbg
		COMMAND ${CMAKE_STRIP} -g $<TARGET_FILE:cimbar_recv2>
	)
endif()

install(
	TARGETS cimbar_recv2
	DESTINATION bin
)

