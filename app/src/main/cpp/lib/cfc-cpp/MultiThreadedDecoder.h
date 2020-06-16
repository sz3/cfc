#pragma once

#include "concurrent_fountain_decoder_sink.h"
#include "encoder/Decoder.h"

#include "concurrent/thread_pool.h"
#include <opencv2/opencv.hpp>

class MultiThreadedDecoder
{
public:
	MultiThreadedDecoder(std::string data_path);

	inline static clock_t bytes = 0;
	inline static clock_t decoded = 0;
	inline static clock_t ticks = 0;

	bool add(const cv::Mat& img, bool should_preprocess);
	bool save(std::string path, const cv::Mat& img);

	void stop();

	unsigned num_threads() const;
	unsigned files_in_flight() const;
	unsigned files_decoded() const;

protected:
	Decoder _dec;
	unsigned _numThreads;
	turbo::thread_pool _pool;
	concurrent_fountain_decoder_sink<626> _writer;
};

inline MultiThreadedDecoder::MultiThreadedDecoder(std::string data_path)
	: _dec()
	, _numThreads(std::max<int>(((int)std::thread::hardware_concurrency())-1, 1))
	, _pool(_numThreads, 1)
	, _writer(data_path)
{
	_pool.start();
}

inline bool MultiThreadedDecoder::add(const cv::Mat& img, bool should_preprocess)
{
	return _pool.try_execute( [&, img, should_preprocess] () {
		clock_t begin = clock();
		bytes += _dec.decode_fountain(img, _writer, should_preprocess);
		++decoded;
		ticks += clock() - begin;
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

inline unsigned MultiThreadedDecoder::files_in_flight() const
{
	return _writer.num_streams();
}

inline unsigned MultiThreadedDecoder::files_decoded() const
{
	return _writer.num_done();
}
