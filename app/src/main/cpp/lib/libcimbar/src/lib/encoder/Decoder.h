#pragma once

#include "reed_solomon_stream.h"
#include "bit_file/bitbuffer.h"
#include "cimb_translator/CimbDecoder.h"
#include "cimb_translator/CimbReader.h"
#include "cimb_translator/Config.h"
#include "cimb_translator/Interleave.h"

#include <opencv2/opencv.hpp>
#include <string>

class Decoder
{
public:
	inline static clock_t readerInitTicks = 0;
	inline static clock_t decodeTicks = 0;
	inline static clock_t fountTicks = 0;
	inline static clock_t bbTicks = 0;
	inline static clock_t rssTicks = 0;

public:
	Decoder(unsigned ecc_bytes=40, unsigned bits_per_op=0, bool interleave=true);

	template <typename MAT, typename STREAM>
	unsigned decode(const MAT& img, STREAM& ostream, bool should_preprocess=false);

	template <typename MAT, typename STREAM>
	unsigned decode_fountain(const MAT& img, STREAM& ostream, bool should_preprocess=false);

	unsigned decode(std::string filename, std::string output);

protected:
	template <typename STREAM>
	unsigned do_decode(CimbReader& reader, STREAM& ostream);

protected:
	unsigned _eccBytes;
	unsigned _bitsPerOp;
	unsigned _interleaveBlocks;
	unsigned _interleavePartitions;
	CimbDecoder _decoder;
};

inline Decoder::Decoder(unsigned ecc_bytes, unsigned bits_per_op, bool interleave)
    : _eccBytes(ecc_bytes)
    , _bitsPerOp(bits_per_op? bits_per_op : cimbar::Config::bits_per_cell())
    , _interleaveBlocks(interleave? cimbar::Config::interleave_blocks() : 0)
    , _interleavePartitions(cimbar::Config::interleave_partitions())
    , _decoder(cimbar::Config::symbol_bits(), cimbar::Config::color_bits(), cimbar::Config::dark(), 0xFF)
{
}

/* while bits == f.read_tile()
 *     decode(bits)
 *
 * bitwriter bw("output.txt");
 * img = open("foo.png")
 * for tileX, tileY in img
 *     bits = decoder.decode(img, tileX, tileY)
 *     bw.write(bits)
 *     if bw.shouldFlush()
 *        bw.flush()
 *
 * */
template <typename STREAM>
inline unsigned Decoder::do_decode(CimbReader& reader, STREAM& ostream)
{
	clock_t begin = clock();

	bitbuffer bb(_bitsPerOp * 1550);
	std::vector<unsigned> interleaveLookup = Interleave::interleave_reverse(reader.num_reads(), _interleaveBlocks, _interleavePartitions);
	while (!reader.done())
	{
		// reader should probably be in charge of the cell index (i) calculation
		// we can compute the bitindex ('index') here, but only the reader will know the right cell index...
		unsigned bits = 0;
		unsigned i = reader.read(bits);
		unsigned bitPos = interleaveLookup[i] * _bitsPerOp;
		bb.write(bits, bitPos, _bitsPerOp);
	}

	bbTicks += clock() - begin;

	begin = clock();
	reed_solomon_stream rss(ostream, _eccBytes);
	unsigned res = bb.flush(rss);
	rssTicks += clock() - begin;
	return res;
}

// seems like we want to take a file or img, and have an output sink
// output sync could be template param?
// we'd decode the full message (via bit_writer) to a temp buffer -- probably a stringstream
// then we'd direct the stringstream to our sink
// which would either be a filestream, or a multi-channel fountain sink

template <typename MAT, typename STREAM>
inline unsigned Decoder::decode(const MAT& img, STREAM& ostream, bool should_preprocess)
{
	CimbReader reader(img, _decoder, should_preprocess);
	return do_decode(reader, ostream);
}

template <typename MAT, typename FOUNTAINSTREAM>
inline unsigned Decoder::decode_fountain(const MAT& img, FOUNTAINSTREAM& ostream, bool should_preprocess)
{
	clock_t begin = clock();
	CimbReader reader(img, _decoder, should_preprocess);
	readerInitTicks += clock() - begin;

	begin = clock();
	std::stringstream buff;
	aligned_stream aligner(buff, ostream.chunk_size());
	unsigned res = do_decode(reader, aligner);
	decodeTicks += clock() - begin;

	begin = clock();
	ostream << buff.str(); // make the buffer contiguous
	fountTicks += clock() - begin;
	return res;
}

inline unsigned Decoder::decode(std::string filename, std::string output)
{
	cv::Mat img = cv::imread(filename);
	std::ofstream f(output);
	return decode(img, f, false);
}
