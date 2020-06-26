#pragma once

#include "concurrent_fountain_decoder_sink.h"
#include "encoder/Decoder.h"
#include "extractor/Anchor.h"
#include "extractor/Deskewer.h"
#include "extractor/Extractor.h"
#include "extractor/Scanner.h"

#include "concurrent/thread_pool.h"
#include <opencv2/opencv.hpp>

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
	bool save(std::string path, const cv::Mat& img);

	void stop();

	int do_extract(const cv::Mat& mat, cv::Mat& img);

	unsigned num_threads() const;
	unsigned backlog() const;
	unsigned files_in_flight() const;
	unsigned files_decoded() const;

protected:
	Decoder _dec;
	unsigned _numThreads;
	turbo::thread_pool _pool;
	concurrent_fountain_decoder_sink<389> _writer;
	std::string _dataPath;
};

inline MultiThreadedDecoder::MultiThreadedDecoder(std::string data_path)
	: _dec(64)
	, _numThreads(std::max<int>(((int)std::thread::hardware_concurrency()/2), 1))
	, _pool(_numThreads, 1)
	, _writer(data_path)
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

	if (anchors.size() < 4)
		return Extractor::FAILURE;

	begin = clock();
	Corners corners(anchors);
	Deskewer de;
	img = de.deskew(mat, corners);
	extractTicks += (clock() - begin);

	cv::rotate(img, img, cv::ROTATE_90_CLOCKWISE);
	cv::cvtColor(img, img, cv::COLOR_RGB2BGR); // opencv JavaCameraView shenanigans defeated?

	return !corners.is_granular_scale(de.total_size());
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

		if (decodeRes >= 5400)
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

inline bool MultiThreadedDecoder::save(std::string path, const cv::Mat& img)
{
	return _pool.try_execute( [&, img, path] () {
		cv::imwrite(path, img);
	} );
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
