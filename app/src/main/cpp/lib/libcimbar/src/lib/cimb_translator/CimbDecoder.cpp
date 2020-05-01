#include "CimbDecoder.h"

#include "Common.h"
#include "CellDrift.h"
#include "image_hash/average_hash.h"
#include "image_hash/hamming_distance.h"
#include "serialize/format.h"

#include <algorithm>
#include <iostream>
using std::string;


static uint64_t _ahashTicks = 0;
static uint64_t _extractAhashTicks = 0;
static uint64_t _bestSymbolTicks = 0;
static uint64_t _bestColorTicks = 0;
static uint64_t _decodeColorTicks = 0;

uint64_t CimbDecoder::ahashTicks()
{
	return _ahashTicks;
}

uint64_t CimbDecoder::extractAhashTicks()
{
	return _extractAhashTicks;
}

uint64_t CimbDecoder::bestSymbolTicks()
{
	return _bestSymbolTicks;
}

uint64_t CimbDecoder::bestColorTicks()
{
	return _bestColorTicks;
}

uint64_t CimbDecoder::decodeColorTicks()
{
	return _decodeColorTicks;
}


CimbDecoder::CimbDecoder(unsigned symbol_bits, unsigned color_bits)
    : _symbolBits(symbol_bits)
    , _numSymbols(1 << symbol_bits)
    , _numColors(1 << color_bits)
    , _dark(true)
{
	load_tiles();
}

uint64_t CimbDecoder::get_tile_hash(unsigned symbol)
{
	cv::Mat tile = cimbar::getTile(_symbolBits, symbol, _dark);
	return image_hash::average_hash(tile);
}

bool CimbDecoder::load_tiles()
{
	unsigned numTiles = _numSymbols;
	for (unsigned i = 0; i < numTiles; ++i)
		_tileHashes.push_back(get_tile_hash(i));
	return true;
}

unsigned CimbDecoder::get_best_symbol(const std::array<uint64_t, 9>& hashes, unsigned& drift_offset)
{
	drift_offset = 0;
	unsigned best_fit = 0;
	unsigned best_distance = 1000;
	// there are 9 candidate hashes. Because we're greedy (see the `return`), we should iterate out from the center
	// 4 == center.
	// 5, 7, 3, 1 == sides.
	// 8, 0, 2, 6 == corners.
	for (unsigned d : {4, 5, 7, 3, 1, 8, 0, 2, 6})
	{
		for (unsigned i = 0; i < _tileHashes.size(); ++i)
		{
			unsigned distance = image_hash::hamming_distance(hashes[d], _tileHashes[i]);
			if (distance < best_distance)
			{
				best_distance = distance;
				best_fit = i;
				drift_offset = d;
				if (best_distance < 8)
					return best_fit;
			}
		}
	}
	return best_fit;
}

unsigned CimbDecoder::decode_symbol(const cv::Mat& cell, unsigned& drift_offset)
{
	clock_t begin = clock();
	auto bits = image_hash::fuzzy_ahash(cell);
	_ahashTicks += clock() - begin;

	begin = clock();
	std::array<uint64_t, 9> hashes = image_hash::extract_fuzzy_ahash(bits);
	/*for (const std::pair<int, int>& drift : CellDrift::driftPairs)
	{
		cv::Rect crop(drift.first + 1, drift.second + 1, 8, 8);
		cv::Mat img = cell(crop);

		hashes.push_back(image_hash::average_hash(img));
	}*/
	_extractAhashTicks += clock() - begin;

	begin = clock();
	unsigned res = get_best_symbol(hashes, drift_offset);
	_bestSymbolTicks += clock() - begin;
	return res;
}

unsigned char CimbDecoder::fix_color(unsigned char c, float adjust) const
{
	return (int)(c * adjust);
}

unsigned CimbDecoder::check_color_distance(cv::Vec3b c, unsigned char r, unsigned char g, unsigned char b) const
{
	return std::pow(c[2] - r, 2) + std::pow(c[1] - g, 2) + std::pow(c[0] - b, 2);
}

unsigned CimbDecoder::get_best_color(unsigned char r, unsigned char g, unsigned char b) const
{
	clock_t begin = clock();
	unsigned char max = std::max(r, g);
	max = std::max(max, b);
	max = std::max(max, uchar(1));

	float adjust = 255.0 / max;
	r = fix_color(r, adjust);
	g = fix_color(g, adjust);
	b = fix_color(b, adjust);

	int best_fit = 0;
	unsigned best_distance = 1000000;
	for (int i = 0; i < _numColors; ++i)
	{
		cv::Vec3b c = cimbar::getColor(i);
		unsigned distance = check_color_distance(c, r, g, b);
		if (distance < best_distance)
		{
			best_fit = i;
			best_distance = distance;
			if (best_distance < 2500)
				break;
		}
	}
	_bestColorTicks += clock() - begin;
	return best_fit;
}

unsigned CimbDecoder::decode_color(const cv::Mat& color_cell, const std::pair<int, int>& drift)
{
	if (_numColors <= 1)
		return 0;

	clock_t begin = clock();
	// limit dimensions to ignore outer row/col
	// when we have the drift, that will factor into this calculation as well
	cv::Rect crop(2 + drift.first, 2 + drift.second, color_cell.cols - 4, color_cell.rows - 4);
	cv::Mat center = color_cell(crop);
	cv::Scalar avgColor = cv::mean(center);
	_decodeColorTicks += clock() - begin;
	return get_best_color(avgColor[2], avgColor[1], avgColor[0]);
}

unsigned CimbDecoder::decode(const cv::Mat& cell, const cv::Mat& color_cell, unsigned& drift_offset)
{
	unsigned bits = decode_symbol(cell, drift_offset);
	bits |= decode_color(color_cell, CellDrift::driftPairs[drift_offset]) << _symbolBits;
	return bits;
}

unsigned CimbDecoder::decode(const cv::Mat& color_cell)
{
	unsigned distance;
	return decode(color_cell, color_cell, distance);
}

