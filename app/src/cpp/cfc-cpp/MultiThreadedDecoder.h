#pragma once

#include "compression/zstd_decompressor.h"
#include "encoder/Decoder.h"
#include "extractor/Anchor.h"
#include "extractor/Deskewer.h"
#include "extractor/Extractor.h"
#include "extractor/Scanner.h"
#include "fountain/concurrent_fountain_decoder_sink.h"

#include "concurrent/thread_pool.h"
#include <opencv2/opencv.hpp>
#include <fstream>

class MultiThreadedDecoder
{
public:
	MultiThreadedDecoder(std::string data_path, int mode_val);

	inline static clock_t count = 0;
	inline static clock_t bytes = 0;
	inline static clock_t perfect = 0;
	inline static clock_t decoded = 0;
	inline static clock_t decodeTicks = 0;
	inline static clock_t scanned = 0;
	inline static clock_t scanTicks = 0;
	inline static clock_t extractTicks = 0;

	bool add(cv::Mat mat);

	void stop();

	int mode() const;
	bool set_mode(int mode_val);
	int detected_mode() const;

	unsigned num_threads() const;
	unsigned backlog() const;
	unsigned files_in_flight() const;
	unsigned files_decoded() const;
	std::vector<std::string> get_done() const;
	std::vector<double> get_progress() const;

protected:
	int do_extract(const cv::Mat& mat, cv::Mat& img);
	void save(const cv::Mat& img);

	static unsigned fountain_chunk_size(int mode_val);

protected:
	int _modeVal;
	int _detectedMode;

	Decoder _dec;
	unsigned _numThreads;
	turbo::thread_pool _pool;
	concurrent_fountain_decoder_sink<cimbar::zstd_decompressor<std::ofstream>> _writer;
	std::string _dataPath;
};

inline MultiThreadedDecoder::MultiThreadedDecoder(std::string data_path, int mode_val)
	: _modeVal(mode_val)
	, _detectedMode(0)
	, _dec(cimbar::Config::ecc_bytes(), cimbar::Config::color_bits())
	, _numThreads(std::max<int>(((int)std::thread::hardware_concurrency()/2), 1))
	, _pool(_numThreads, 1)
	, _writer(data_path, fountain_chunk_size(mode_val))
	, _dataPath(data_path)
{
	FountainInit::init();
	_pool.start();
}

inline int MultiThreadedDecoder::do_extract(const cv::Mat& mat, cv::Mat& img)
{
	clock_t begin = clock();

	Scanner scanner(mat);
	std::vector<Anchor> anchors = scanner.scan();
	++scanned;
	scanTicks += (clock() - begin);

	//if (anchors.size() >= 3) save(mat);

	if (anchors.size() < 4)
		return Extractor::FAILURE;

	begin = clock();
	Corners corners(anchors);
	Deskewer de;
	img = de.deskew(mat, corners);
	extractTicks += (clock() - begin);

	return Extractor::SUCCESS;
}

inline bool MultiThreadedDecoder::add(cv::Mat mat)
{
    ++count;
    bool legacy_mode = _modeVal == 4 or (_modeVal == 0 and count%2 == 0);
    return _pool.try_execute( [&, mat, legacy_mode] () {
		cv::Mat img;
		int res = do_extract(mat, img);
		if (res == Extractor::FAILURE)
			return;

		// if extracted image is small, we'll need to run some filters on it
		clock_t begin = clock();
		bool should_preprocess = (res == Extractor::NEEDS_SHARPEN);
		int color_correction = legacy_mode? 1 : 2;
		unsigned color_mode = legacy_mode? 0 : 1;
		unsigned decodeRes = _dec.decode_fountain(img, _writer, color_mode, should_preprocess, color_correction);
		bytes += decodeRes;
		++decoded;
		decodeTicks += clock() - begin;

		if (decodeRes and _modeVal == 0)
            _detectedMode = legacy_mode? 4 : 68;

		if (decodeRes >= 6900)
			++perfect;
	} );
}

inline void MultiThreadedDecoder::save(const cv::Mat& mat)
{
	std::stringstream fname;
	fname << _dataPath << "/scan" << (scanned-1) << ".png";
	cv::Mat bgr;
	cv::cvtColor(mat, bgr, cv::COLOR_RGB2BGR);
	cv::imwrite(fname.str(), bgr);
}

inline void MultiThreadedDecoder::stop()
{
	_pool.stop();
}

unsigned MultiThreadedDecoder::fountain_chunk_size(int mode_val)
{
	return cimbar::Config::fountain_chunk_size(cimbar::Config::ecc_bytes(), cimbar::Config::symbol_bits() + cimbar::Config::color_bits(), mode_val==4);
}

inline int MultiThreadedDecoder::mode() const
{
	return _modeVal;
}

inline bool MultiThreadedDecoder::set_mode(int mode_val)
{
	if (_modeVal == mode_val)
		return true;

	if (mode_val != 0 and _writer.chunk_size() != fountain_chunk_size(mode_val))
		return false; // if so, we need to reset to change it

	// reset detectedMode iff we're switching back to autodetect
	if (mode_val == 0)
		_detectedMode = 0;

	_modeVal = mode_val;
	return true;
}

inline int MultiThreadedDecoder::detected_mode() const
{
	return _detectedMode;
}

inline unsigned MultiThreadedDecoder::num_threads() const
{
	return _numThreads;
}

inline unsigned MultiThreadedDecoder::backlog() const
{
	return _pool.queued();
}

inline unsigned MultiThreadedDecoder::files_in_flight() const
{
	return _writer.num_streams();
}

inline unsigned MultiThreadedDecoder::files_decoded() const
{
	return _writer.num_done();
}

inline std::vector<std::string> MultiThreadedDecoder::get_done() const
{
	return _writer.get_done();
}

inline std::vector<double> MultiThreadedDecoder::get_progress() const
{
	return _writer.get_progress();
}
