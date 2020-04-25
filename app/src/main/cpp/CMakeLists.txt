cmake_minimum_required(VERSION 3.4.1)

# opencv stuff
add_library( lib_opencv SHARED IMPORTED )
set_target_properties(lib_opencv PROPERTIES IMPORTED_LOCATION ${OpenCV_DIR}/libs/${ANDROID_ABI}/libopencv_java4.so)

# our stuff
set (PROJECTS
   lib/native-lib
)

include_directories(
        ${OpenCV_DIR}/jni/include
)

foreach(proj ${PROJECTS})
	add_subdirectory(${proj} build/${proj})
endforeach()

