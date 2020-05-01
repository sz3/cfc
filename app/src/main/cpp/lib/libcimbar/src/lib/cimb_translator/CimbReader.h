#pragma once

#include "CellDrift.h"
#include "CellPosition.h"
#include "CimbDecoder.h"

#include <opencv2/opencv.hpp>
#include <string>

class CimbReader
{
public:
	static uint64_t getTicksA();
	static uint64_t getTicksB();
	static uint64_t getTicksC();

	CimbReader(std::string filename);
	CimbReader(const cv::Mat& img);

	unsigned read();
	bool done() const;

protected:
	cv::Mat _image;
	cv::Mat _grayscale;
	unsigned _cellSize;
	CellPosition _position;
	CellDrift _drift;
	CimbDecoder _decoder;
};
