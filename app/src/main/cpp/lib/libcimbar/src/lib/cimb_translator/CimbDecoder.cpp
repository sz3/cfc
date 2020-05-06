#include "CimbDecoder.h"

#include "Common.h"
#include "CellDrift.h"
#include "image_hash/average_hash.h"
#include "image_hash/hamming_distance.h"
#include "serialize/format.h"
#include "util/Timer.h"

#include <algorithm>
#include <iostream>
#include <tuple>
using std::get;
using std::string;


static clock_t _decodeSymbolTicks = 0;
static clock_t _decodeColorTicks = 0;
static clock_t _ahashTicks = 0;
static clock_t _extractAhashTicks = 0;
static clock_t _bestSymbolTicks = 0;
static clock_t _avgColorTicks = 0;
static clock_t _bestColorTicks = 0;

clock_t CimbDecoder::decodeSymbolTicks()
{
	return _decodeSymbolTicks;
}

clock_t CimbDecoder::decodeColorTicks()
{
	return _decodeColorTicks;
}

clock_t CimbDecoder::ahashTicks()
{
	return _ahashTicks;
}

clock_t CimbDecoder::extractAhashTicks()
{
	return _extractAhashTicks;
}

clock_t CimbDecoder::bestSymbolTicks()
{
	return _bestSymbolTicks;
}

clock_t CimbDecoder::avgColorTicks()
{
	return _avgColorTicks;
}

clock_t CimbDecoder::bestColorTicks()
{
	return _bestColorTicks;
}


namespace {
	std::tuple<uchar,uchar,uchar> mean_color(const cv::Mat& img, int xstart, int ystart, int cols, int rows)
	{
		cols = cols * img.channels();
		ystart = ystart * img.channels();

		unsigned blue = 0;
		unsigned green = 0;
		unsigned red = 0;
		unsigned count = 0;

		int i,j;
		for( i = xstart; i < rows; ++i)
		{
			const uchar* p = img.ptr<uchar>(i);
			for (j = ystart; j < cols; j+=3)
			{
				blue += p[j];
				green += p[j+1];
				red += p[j+2];
				count += 1;
			}
		}

		if (!count)
			return std::tuple<uchar,uchar,uchar>(0, 0, 0);

		return std::tuple<uchar,uchar,uchar>(red/count, green/count, blue/count);
	}
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
	Timer dst(_decodeSymbolTicks);

	clock_t begin = clock();
	auto bits = image_hash::fuzzy_ahash(cell);
	_ahashTicks += clock() - begin;

	begin = clock();
	unsigned res;
	{
		std::array<uint64_t, 9> hashes = image_hash::extract_fuzzy_ahash(bits);
		/*for (const std::pair<int, int>& drift : CellDrift::driftPairs)
		{
			cv::Rect crop(drift.first + 1, drift.second + 1, 8, 8);
			cv::Mat img = cell(crop);

			hashes.push_back(image_hash::average_hash(img));
		}*/
		_extractAhashTicks += clock() - begin;

		begin = clock();
		res = get_best_symbol(hashes, drift_offset);
	}
	_bestSymbolTicks += clock() - begin;
	return res;
}

inline unsigned char CimbDecoder::fix_color(unsigned char c, float adjust) const
{
	return (uchar)(c * adjust);
}

inline unsigned CimbDecoder::check_color_distance(std::tuple<uchar,uchar,uchar> c, unsigned char r, unsigned char g, unsigned char b) const
{
	return std::pow(get<0>(c) - r, 2) + std::pow(get<1>(c) - g, 2) + std::pow(get<2>(c) - b, 2);
}

inline unsigned CimbDecoder::get_best_color(unsigned char r, unsigned char g, unsigned char b) const
{
	Timer t(_bestColorTicks);
	unsigned char max = std::max({r, g, b, uchar(1)});

	float adjust = 255.0 / max;
	r = fix_color(r, adjust);
	g = fix_color(g, adjust);
	b = fix_color(b, adjust);

	unsigned best_fit = 0;
	unsigned best_distance = 1000000;
	for (int i = 0; i < _numColors; ++i)
	{
		std::tuple<uchar,uchar,uchar> c = cimbar::getColor(i);
		unsigned distance = check_color_distance(c, r, g, b);
		if (distance < best_distance)
		{
			best_fit = i;
			best_distance = distance;
			if (best_distance < 2500)
				break;
		}
	}
	return best_fit;
}

unsigned CimbDecoder::decode_color(const cv::Mat& color_cell, const std::pair<int, int>& drift)
{
	if (_numColors <= 1)
		return 0;

	Timer t(_decodeColorTicks);
	uchar r,g,b;
	{
		Timer t2(_avgColorTicks);
		// limit dimensions to ignore outer row/col. We want a 6x6
		std::tie(r, g, b) = mean_color(color_cell, 2+drift.first, 2+drift.second, color_cell.cols-4, color_cell.rows-4);
	}

	return get_best_color(r, g, b);
}

unsigned CimbDecoder::decode(const cv::Mat& cell, const cv::Mat& color_cell, unsigned& drift_offset)
{
	Timer t(decodeTicksA);
	unsigned bits = decode_symbol(cell, drift_offset);
	bits |= decode_color(color_cell, CellDrift::driftPairs[drift_offset]) << _symbolBits;
	return bits;
}

unsigned CimbDecoder::decode(const cv::Mat& color_cell)
{
	unsigned distance;
	return decode(color_cell, color_cell, distance);
}

