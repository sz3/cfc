#include "Decoder.h"

#include "ReedSolomonFile.h"
#include "bit_file/bitwriter.h"
#include "cimb_translator/CimbReader.h"
#include "cimb_translator/Config.h"
#include "util/Timer.h"

#include <string>
using std::string;
using namespace cimbar;


static clock_t _ticks = 0;
static clock_t _bytesWritten = 0;


namespace {
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
	unsigned do_decode(CimbReader& reader, unsigned ecc_bytes, string output, unsigned bits_per_op)
	{
		Timer t(_ticks);

		bitwriter<> bw;
		ReedSolomonFile f(output, ecc_bytes, true);

		unsigned bytesWritten = 0;
		while (!reader.done())
		{
			unsigned bits = reader.read();
			bw.write(bits, bits_per_op);
			if (bw.shouldFlush())
				bytesWritten += bw.flush(f);
		}

		// flush once more
		bytesWritten += bw.flush(f);

		_bytesWritten += bytesWritten;
		return bytesWritten;
	}
}

clock_t Decoder::getTicks()
{
	return _ticks;
}

clock_t Decoder::getBytesWritten()
{
	return _bytesWritten;
}

Decoder::Decoder(unsigned ecc_bytes, unsigned bits_per_op)
    : _eccBytes(ecc_bytes)
    , _bitsPerOp(bits_per_op? bits_per_op : Config::bits_per_cell())
    , _decoder(Config::symbol_bits(), Config::color_bits())
{
}

unsigned Decoder::decode(const cv::Mat& img, std::string output)
{
	CimbReader reader(img, _decoder);
	return do_decode(reader, _eccBytes, output, _bitsPerOp);
}

unsigned Decoder::decode(string filename, string output)
{
	CimbReader reader(filename, _decoder);
	return do_decode(reader, _eccBytes, output, _bitsPerOp);
}
