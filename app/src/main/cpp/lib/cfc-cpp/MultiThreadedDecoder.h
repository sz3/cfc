#pragma once

#include "encoder/Decoder.h"
#include "extractor/Extractor.h"
#include "concurrent/thread_pool.h"
#include <opencv2/opencv.hpp>

class MultiThreadedDecoder
{
public:
	MultiThreadedDecoder();

	inline static clock_t bytes = 0;
	inline static clock_t decoded = 0;

	bool add(const cv::Mat& img);
	cv::Mat pop();

	void stop();

protected:
	Extractor _ext;
	Decoder _dec;
	turbo::thread_pool _pool;
};

inline MultiThreadedDecoder::MultiThreadedDecoder()
	: _ext()
	, _dec(0)
	, _pool(std::thread::hardware_concurrency(), 1)
{
	_pool.start();
}

inline bool MultiThreadedDecoder::add(const cv::Mat& img)
{
	return _pool.try_execute( [&, img] () {
		bytes += _dec.decode(img, "/run/shm/notyet.txt");
		++decoded;
	} );
}

inline void MultiThreadedDecoder::stop()
{
	_pool.stop();
}

