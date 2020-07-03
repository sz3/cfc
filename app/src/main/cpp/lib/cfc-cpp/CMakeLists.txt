cmake_minimum_required(VERSION 3.4.1)

add_library(cfc-cpp
        SHARED

        jni.cpp
        concurrent_fountain_decoder_sink.h
        MultiThreadedDecoder.h
)

find_library(log-lib

        log
)

target_link_libraries(cfc-cpp

-Wl,--start-group
        cimb_translator
	extractor
-Wl,--end-group

        correct
        wirehair
        ${OPENCV_LIBS}
        ${log-lib}
)
