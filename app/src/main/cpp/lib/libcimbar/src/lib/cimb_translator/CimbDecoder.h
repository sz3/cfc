#pragma once

#include "CellDrift.h"
#include "util/Timer.h"
#include <opencv2/opencv.hpp>
#include <cstdint>
#include <string>

class CimbDecoder
{
public:
	inline static clock_t decodeTicksA = 0;
	static clock_t decodeSymbolTicks();
	static clock_t decodeColorTicks();
	static clock_t ahashTicks();
	static clock_t extractAhashTicks();
	static clock_t bestSymbolTicks();
	static clock_t avgColorTicks();
	static clock_t bestColorTicks();

	CimbDecoder(unsigned symbol_bits, unsigned color_bits);

	unsigned decode(const cv::Mat& color_cell) const;
	unsigned decode(const cv::Mat& cell, const cv::Mat& color_cell, unsigned& drift_offset) const;

	unsigned get_best_symbol(const std::array<uint64_t,9>& hashes, unsigned& best_distance) const;
	unsigned decode_symbol(const cv::Mat& cell, unsigned& drift_offset) const;

	inline unsigned get_best_color(unsigned char r, unsigned char g, unsigned char b) const;
	unsigned decode_color(const cv::Mat& cell, const std::pair<int, int>& drift) const;

protected:
	uint64_t get_tile_hash(unsigned symbol) const;
	bool load_tiles();

	unsigned check_color_distance(std::tuple<uchar,uchar,uchar> c, unsigned char r, unsigned char g, unsigned char b) const;
	inline unsigned char fix_color(unsigned char c, float adjust) const;

protected:
	std::vector<uint64_t> _tileHashes;
	unsigned _symbolBits;
	unsigned _numSymbols;
	unsigned _numColors;
	bool _dark;
};

