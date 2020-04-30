cmake_minimum_required(VERSION 3.4.1)

add_library(native-lib
        SHARED

        native-lib.cpp
        Processing.h
)

find_library(log-lib

        log
)

target_link_libraries(native-lib

-Wl,--start-group
        cimb_translator
	encoder
	extractor
-Wl,--end-group

        correct
        ${OPENCV_LIBS}
        ${log-lib}
)
