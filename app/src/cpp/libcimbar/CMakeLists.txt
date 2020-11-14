cmake_minimum_required(VERSION 3.10)

project ( libcimbar )
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set(CMAKE_INSTALL_PREFIX "${libcimbar_SOURCE_DIR}/dist" CACHE PATH "..." FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)

if(NOT DEFINED OPENCV_LIBS)
	set(OPENCV_LIBS "opencv_core" "opencv_imgcodecs" "opencv_imgproc" "opencv_photo")
endif()

if(NOT DEFINED CPPFILESYSTEM)
	set(CPPFILESYSTEM "stdc++fs")
endif()

if(DEFINED USE_GLFW)
	set(libcimbar_UI_LIB "glfw" "GL")
	add_definitions("-DLIBCIMBAR_USE_GLFW=${USE_GLFW}")
else()
	set(libcimbar_UI_LIB "opencv_highgui")
endif()

set (PROJECTS
	src/lib/bit_file
	src/lib/cimb_translator
	src/lib/compression
	src/lib/encoder
	src/lib/fountain
	src/lib/gui
	src/lib/image_hash
	src/lib/serialize
	src/lib/util

	src/third_party_lib/base91
	src/third_party_lib/cxxopts
	src/third_party_lib/intx
	src/third_party_lib/libcorrect
	src/third_party_lib/libpopcnt
	src/third_party_lib/wirehair
	src/third_party_lib/zstd
)

if(NOT DEFINED USE_WASM)
set(PROJECTS
	${PROJECTS}
	src/lib/extractor
)
endif()

if (DEFINED USE_WASM OR USE_GLFW)
set(PROJECTS
	${PROJECTS}
	src/wasm/cimbar_js
)
endif()

include_directories(
	${libcimbar_SOURCE_DIR}/src/lib
	${libcimbar_SOURCE_DIR}/src/third_party_lib
)

foreach(proj ${PROJECTS})
	add_subdirectory(${proj} build/${proj})
endforeach()


