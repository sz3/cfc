#include "CimbReader.h"

#include "CellDrift.h"
#include "Config.h"
#include <opencv2/opencv.hpp>

using namespace cimbar;

static uint64_t _ticksA = 0;
static uint64_t _ticksB = 0;
static uint64_t _ticksC = 0;

CimbReader::CimbReader(std::string filename, const CimbDecoder& decoder)
    : CimbReader(cv::imread(filename), decoder)
{}

CimbReader::CimbReader(const cv::Mat& img, const CimbDecoder& decoder)
    : _image(img)
    , _cellSize(Config::cell_size() + 2)
    , _position(Config::cell_spacing(), Config::num_cells(), Config::cell_size(), Config::corner_padding())
    , _drift()
    , _decoder(decoder)
{
	cv::cvtColor(_image, _grayscale, cv::COLOR_BGR2GRAY);
}

uint64_t CimbReader::getTicksA()
{
	return _ticksA;
}

uint64_t CimbReader::getTicksB()
{
	return _ticksB;
}

uint64_t CimbReader::getTicksC()
{
	return _ticksC;
}

unsigned CimbReader::read()
{
	if (_position.done())
		return 0;

	clock_t begin = clock();
	_ticksA += clock() - begin;
	const CellPosition::coordinate& xy = _position.next();
	int x = xy.first + _drift.x();
	int y = xy.second + _drift.y();
	begin = clock();
	cv::Rect crop(x-1, y-1, _cellSize, _cellSize);
	cv::Mat cell = _grayscale(crop);
	cv::Mat color_cell = _image(crop);
	_ticksB += clock() - begin;

	begin = clock();
	unsigned drift_offset = 0;
	unsigned bits = _decoder.decode(cell, color_cell, drift_offset);

	std::pair<int, int> best_drift = CellDrift::driftPairs[drift_offset];
	_drift.updateDrift(best_drift.first, best_drift.second);

	_ticksC += clock() - begin;
	return bits;
}

bool CimbReader::done() const
{
	return _position.done();
}
