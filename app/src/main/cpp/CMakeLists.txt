cmake_minimum_required(VERSION 3.4.1)

project ( cfc )

# opencv stuff
add_library( lib_opencv SHARED IMPORTED )
set_target_properties(lib_opencv PROPERTIES IMPORTED_LOCATION ${OpenCV_DIR}/libs/${ANDROID_ABI}/libopencv_java4.so)

set( DISABLE_TESTS true )
set( OPENCV_LIBS "lib_opencv" )

# our stuff
set (PROJECTS
   lib/cfc-cpp
   lib/libcimbar
)

include_directories(
        ${OpenCV_DIR}/jni/include
        lib/libcimbar/src/lib
)

foreach(proj ${PROJECTS})
	add_subdirectory(${proj} build/${proj})
endforeach()

