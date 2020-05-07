#pragma once

#include "encoder/Decoder.h"
#include "extractor/Extractor.h"
#include "concurrent/thread_pool.h"
#include <opencv2/opencv.hpp>

class MultiThreadedDecoder
{
public:
	MultiThreadedDecoder();

	bool add(cv::Mat img);
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

inline bool MultiThreadedDecoder::add(cv::Mat img)
{
	return _pool.try_execute( [&, img] () {
		cv::Mat& working = const_cast<cv::Mat&>(img);
		if (!_ext.extract(working, working))
			return;
		_dec.decode(working, "/run/shm/notyet.txt");
	} );
}

inline void MultiThreadedDecoder::stop()
{
	_pool.stop();
}

