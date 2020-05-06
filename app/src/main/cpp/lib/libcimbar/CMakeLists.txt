cmake_minimum_required(VERSION 3.10)

project ( libcimbar )
set(MINIMAL_BUILD "FALSE" CACHE BOOL "build all components")


if(NOT DEFINED OPENCV_LIBS)
	set(OPENCV_LIBS "opencv_core" "opencv_imgcodecs" "opencv_imgproc" "opencv_photo")
endif()

set (PROJECTS
	src/exe/build_image_assets

	src/lib/bit_file
	src/lib/cimb_translator
	src/lib/encoder
	src/lib/extractor
	src/lib/image_hash
	src/lib/serialize
	src/lib/util

	src/third_party_lib/base91
	src/third_party_lib/cxxopts
	src/third_party_lib/intx
	src/third_party_lib/libcorrect
	src/third_party_lib/libpopcnt
)

if (NOT MINIMAL_BUILD)
set (PROJECTS
	${PROJECTS}
   	src/lib/concurrent
	src/third_party_lib/concurrentqueue
)
endif()

include_directories(
	${libcimbar_SOURCE_DIR}/src/lib
	${libcimbar_SOURCE_DIR}/src/third_party_lib
)

foreach(proj ${PROJECTS})
	add_subdirectory(${proj} build/${proj})
endforeach()

