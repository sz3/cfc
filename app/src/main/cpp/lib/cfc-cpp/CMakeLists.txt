cmake_minimum_required(VERSION 3.4.1)

add_library(cfc-cpp
        SHARED

        jni.cpp
        Processing.h
)

find_library(log-lib

        log
)

target_link_libraries(cfc-cpp

-Wl,--start-group
        cimb_translator
	encoder
	extractor
-Wl,--end-group

        correct
        ${OPENCV_LIBS}
        ${log-lib}
)
