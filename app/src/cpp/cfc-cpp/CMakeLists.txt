cmake_minimum_required(VERSION 3.4.1)

add_library(cfc-cpp
        SHARED

        jni.cpp
        MultiThreadedDecoder.h
)

find_library(log-lib

        log
)

target_link_libraries(cfc-cpp

        cimb_translator
	extractor

        correct_static
        wirehair
        zstd
        ${OPENCV_LIBS}
        ${log-lib}
)

target_compile_options(cfc-cpp  PUBLIC "-DZSTD_STATIC_LINKING_ONLY")
