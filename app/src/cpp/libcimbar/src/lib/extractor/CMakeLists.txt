cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -mllvm -inline-threshold=1500")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mllvm -inline-threshold=1500")

set(SOURCES
	Anchor.h
	Corners.h
	Deskewer.cpp
	Deskewer.h
	DistortionParameters.h
	EdgeScanState.h
	Extractor.cpp
	Extractor.h
	Geometry.h
	Midpoints.h
	Point.h
	ScanState.h
	Scanner.cpp
	Scanner.h
	SimpleCameraCalibration.cpp
	SimpleCameraCalibration.h
	Undistort.h
)

add_library(extractor STATIC ${SOURCES})

if(NOT DEFINED DISABLE_TESTS)
	add_subdirectory(test)
endif()
