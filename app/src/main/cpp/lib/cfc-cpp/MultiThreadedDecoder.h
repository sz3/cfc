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
	MultiThreadedDecoder(std::string data_path);

	inline static clock_t bytes = 0;
	inline static clock_t perfect = 0;
	inline static clock_t decoded = 0;
	inline static clock_t decodeTicks = 0;
	inline static clock_t scanned = 0;
	inline static clock_t scanTicks = 0;
	inline static clock_t extractTicks = 0;

	bool add(cv::Mat mat);
	bool decode(const cv::Mat& img, bool should_preprocess);

	void stop();

	unsigned num_threads() const;
	unsigned backlog() const;
	unsigned files_in_flight() const;
	unsigned files_decoded() const;

protected:
	int do_extract(const cv::Mat& mat, cv::Mat& img);
	void save(const cv::Mat& img);

protected:
	Decoder _dec;
	unsigned _numThreads;
	turbo::thread_pool _pool;
	concurrent_fountain_decoder_sink<cimbar::zstd_decompressor<std::ofstream>> _writer;
	std::string _dataPath;
};

inline MultiThreadedDecoder::MultiThreadedDecoder(std::string data_path)
	: _dec(cimbar::Config::ecc_bytes())
	, _numThreads(std::max<int>(((int)std::thread::hardware_concurrency()/2), 1))
	, _pool(_numThreads, 1)
	, _writer(data_path, cimbar::Config::fountain_chunk_size(cimbar::Config::ecc_bytes()))
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

	//cv::cvtColor(img, img, cv::COLOR_RGB2BGR); // opencv JavaCameraView shenanigans defeated?
	extractTicks += (clock() - begin);

	return Extractor::SUCCESS;
}

inline bool MultiThreadedDecoder::add(cv::Mat mat)
{
	return _pool.try_execute( [&, mat] () {
		cv::Mat img;
		int res = do_extract(mat, img);
		if (res == Extractor::FAILURE)
			return;

		// if extracted image is small, we'll need to run some filters on it
		clock_t begin = clock();
		bool should_preprocess = (res == Extractor::NEEDS_SHARPEN);
		unsigned decodeRes = _dec.decode_fountain(img, _writer, should_preprocess);
		bytes += decodeRes;
		++decoded;
		decodeTicks += clock() - begin;

		if (decodeRes >= 6900)
			++perfect;
	} );
}

inline bool MultiThreadedDecoder::decode(const cv::Mat& img, bool should_preprocess)
{
	return _pool.try_execute( [&, img, should_preprocess] () {
		clock_t begin = clock();
		bytes += _dec.decode_fountain(img, _writer, should_preprocess);
		++decoded;
		decodeTicks += clock() - begin;
	} );
}

inline void MultiThreadedDecoder::save(const cv::Mat& mat)
{
	std::stringstream fname;
	fname << _dataPath << "/scan" << (scanned-1) << ".png";
	cv::imwrite(fname.str(), mat);
}

inline void MultiThreadedDecoder::stop()
{
	_pool.stop();
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
