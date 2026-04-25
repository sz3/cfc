cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -mllvm -inline-threshold=1500")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mllvm -inline-threshold=1500")

set(SOURCES
	bitmaps.h
	AdjacentCellFinder.cpp
	AdjacentCellFinder.h
	Cell.h
	CellDrift.cpp
	CellDrift.h
	CellPositions.cpp
	CellPositions.h
	CimbDecoder.cpp
	CimbDecoder.h
	CimbEncoder.cpp
	CimbEncoder.h
	CimbReader.cpp
	CimbReader.h
	CimbWriter.cpp
	CimbWriter.h
	Common.cpp
	Common.h
	Config.h
	FloodDecodePositions.cpp
	FloodDecodePositions.h
	GridConf.h
	Interleave.h
	LinearDecodePositions.h
	PositionData.h
)

add_library(cimb_translator STATIC ${SOURCES})

if(NOT DEFINED DISABLE_TESTS)
	add_subdirectory(test)
endif()
