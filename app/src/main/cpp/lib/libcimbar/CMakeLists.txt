cmake_minimum_required(VERSION 3.10)

project ( libcimbar )

if(NOT DEFINED OPENCV_LIBS)
	set(OPENCV_LIBS "opencv_core" "opencv_imgcodecs" "opencv_imgproc" "opencv_photo")
endif()

set (PROJECTS
	src/exe/build_image_assets

	src/lib/bit_file
	src/lib/cimb_translator
	src/lib/encoder
	src/lib/extractor
	src/lib/fountain
	src/lib/image_hash
	src/lib/serialize
	src/lib/util

	src/third_party_lib/base91
	src/third_party_lib/cxxopts
	src/third_party_lib/intx
	src/third_party_lib/libcorrect
	src/third_party_lib/libpopcnt
	src/third_party_lib/wirehair
)

include_directories(
	${libcimbar_SOURCE_DIR}/src/lib
	${libcimbar_SOURCE_DIR}/src/third_party_lib
)

foreach(proj ${PROJECTS})
	add_subdirectory(${proj} build/${proj})
endforeach()

