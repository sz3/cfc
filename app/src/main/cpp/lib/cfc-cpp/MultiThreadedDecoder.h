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
protected:
	using umat_pair = std::pair<cv::UMat, cv::UMat>;

public:
	MultiThreadedDecoder(std::string data_path);

	inline static clock_t bytes = 0;
	inline static clock_t perfect = 0;
	inline static clock_t decoded = 0;
	inline static clock_t decodeTicks = 0;
	inline static clock_t scanned = 0;
	inline static clock_t scanTicks = 0;
	inline static clock_t extracted = 0;
	inline static clock_t extractTicks = 0;

	inline static clock_t gpuToTicks = 0;
	inline static clock_t gpuFromTicks = 0;

	inline static std::array<cv::UMat, 3> _decoderRing;
	inline static unsigned _di = 0;

	bool add(cv::Mat mat);
	bool decode(const cv::Mat& img, bool should_preprocess);

	void stop();

	unsigned num_threads() const;
	unsigned backlog() const;
	unsigned files_in_flight() const;
	unsigned files_decoded() const;
	std::vector<std::string> get_done() const;
	std::vector<double> get_progress() const;

protected:
	int do_extract(const cv::Mat& mat, cv::UMat& out);
	unsigned do_decode(cv::UMat& img);
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
	, _numThreads(1)
	, _pool(_numThreads, 1)
	, _writer(data_path, cimbar::Config::fountain_chunk_size(cimbar::Config::ecc_bytes()))
	, _dataPath(data_path)
{
	FountainInit::init();
	_pool.start();
}

inline int MultiThreadedDecoder::do_extract(const cv::Mat& mat, cv::UMat& out)
{
	++scanned;
	clock_t begin = clock();

	Scanner scanner(mat);
	std::vector<Anchor> anchors = scanner.scan();
	scanTicks += (clock() - begin);

	//if (anchors.size() >= 3) save(mat);

	if (anchors.size() < 4)
		return Extractor::FAILURE;

	++extracted;
	begin = clock();
	cv::UMat gpuImg = mat.getUMat(cv::ACCESS_RW);
	gpuToTicks += clock() - begin;

	begin = clock();
	Corners corners(anchors);
	Deskewer de;
	out = de.deskew(gpuImg, corners);
	extractTicks += (clock() - begin);

	return Extractor::SUCCESS;
}

inline unsigned MultiThreadedDecoder::do_decode(cv::UMat& img)
{
	++decoded;
	clock_t begin = clock();
	cv::Mat cpuImg = img.getMat(cv::ACCESS_FAST);
	gpuFromTicks += clock() - begin;

	begin = clock();
	unsigned decodeRes = _dec.decode_fountain(cpuImg, _writer, false);
	decodeTicks += clock() - begin;
	return decodeRes;
}

inline bool MultiThreadedDecoder::add(cv::Mat mat)
{
	// runs once per frame
	// but the frame we attempt to decode will be an older one...
	return _pool.try_execute( [&, mat] () {
		// decode half
		cv::UMat& current = _decoderRing[_di];
		if (current.cols > 0 and current.rows > 0)
		{
			unsigned decodeRes = do_decode(current);
			bytes += decodeRes;
			if (decodeRes >= 6900)
				++perfect;
			current.release();
		}

		// extract half. Fill up the ring for the next decode.
		cv::UMat img;
		int res = do_extract(mat, img);
		if (res != Extractor::FAILURE)
		{
			_decoderRing[_di] = img;
			if (++_di >= _decoderRing.size())
				_di = 0;
		}
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
	cv::Mat bgr;
	cv::cvtColor(mat, bgr, cv::COLOR_RGB2BGR);
	cv::imwrite(fname.str(), bgr);
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

inline std::vector<std::string> MultiThreadedDecoder::get_done() const
{
	return _writer.get_done();
}

inline std::vector<double> MultiThreadedDecoder::get_progress() const
{
	return _writer.get_progress();
}
