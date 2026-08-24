cmake_minimum_required(VERSION 3.10)

project(cimbar_send)

set (SOURCES
	send.cpp
)

add_executable (
	cimbar_send
	${SOURCES}
)

target_link_libraries(cimbar_send

	cimbar_js
)

if(MSVC)
	add_custom_command(
		TARGET cimbar_send POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			$<TARGET_FILE:unofficial::angle::libEGL>
			$<TARGET_FILE_DIR:cimbar_send>
	)
else()
	add_custom_command(
		TARGET cimbar_send POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:cimbar_send> cimbar_send.dbg
		COMMAND ${CMAKE_STRIP} -g $<TARGET_FILE:cimbar_send>
	)
endif()

install(
	TARGETS cimbar_send
	DESTINATION bin
)

