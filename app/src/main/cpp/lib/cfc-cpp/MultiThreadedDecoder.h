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
	using umat_pair = std::pair<cv::UMat, std::vector<Anchor>>;

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
	inline static clock_t gpup = 0;
	inline static clock_t gpuToTicks = 0;
	inline static clock_t gpuf = 0;
	inline static clock_t gpuFromTicks = 0;

	inline static std::array<cv::UMat, 32> _decoderRing;
	inline static std::array<umat_pair, 10> _inRing;
	inline static unsigned _ii = 0;

	bool add(cv::Mat mat, int index);
	bool decode(const cv::Mat& img, bool should_preprocess);

	void stop();

	unsigned num_threads() const;
	unsigned backlog() const;
	unsigned files_in_flight() const;
	unsigned files_decoded() const;
	std::vector<std::string> get_done() const;
	std::vector<double> get_progress() const;

protected:
	std::vector<Anchor> do_scan(const cv::Mat& img);
	unsigned do_decode(cv::Mat img);
	void save(const cv::Mat& img);

protected:
	void gpu_extract(const cv::UMat& mat, const std::vector<Anchor>& anchors, int index);
	void gpu_schedule_decode(int index);

protected:
	Decoder _dec;
	unsigned _numThreads;
	turbo::thread_pool _pool;
	turbo::thread_pool _gpuPool;
	concurrent_fountain_decoder_sink<cimbar::zstd_decompressor<std::ofstream>> _writer;
	std::string _dataPath;
};

inline MultiThreadedDecoder::MultiThreadedDecoder(std::string data_path)
	: _dec(cimbar::Config::ecc_bytes())
	, _numThreads(1)
	, _pool(_numThreads, 1)
	, _gpuPool(1, 1)
	, _writer(data_path, cimbar::Config::fountain_chunk_size(cimbar::Config::ecc_bytes()))
	, _dataPath(data_path)
{
	FountainInit::init();
	_pool.start();
	_gpuPool.start();
}

inline std::vector<Anchor> MultiThreadedDecoder::do_scan(const cv::Mat& img)
{
	++scanned;
	clock_t begin = clock();

	Scanner scanner(img);
	std::vector<Anchor> anchors = scanner.scan();
	scanTicks += (clock() - begin);

	//if (anchors.size() >= 3) save(mat);

	return anchors;
}

inline void MultiThreadedDecoder::gpu_extract(const cv::UMat& gpuImg, const std::vector<Anchor>& anchors, int index)
{
	++extracted;

	clock_t begin = clock();
	Corners corners(anchors);
	Deskewer de;
	cv::UMat out = de.deskew(gpuImg, corners);
	extractTicks += (clock() - begin);

	_decoderRing[index] = out;
}

inline unsigned MultiThreadedDecoder::do_decode(cv::Mat img)
{
	++decoded;

	clock_t begin = clock();
	unsigned decodeRes = _dec.decode_fountain(img, _writer, false);
	decodeTicks += clock() - begin;
	bytes += decodeRes;
	if (decodeRes >= 6900)
		++perfect;

	return decodeRes;
}

void MultiThreadedDecoder::gpu_schedule_decode(int index)
{
	++gpuf;

	// decode half
	cv::UMat& current = _decoderRing[index];
	if (current.cols <= 0 or current.rows <= 0)
		return;

	clock_t begin = clock();
	cv::Mat cpuImg = current.getMat(cv::ACCESS_FAST);
	gpuFromTicks += clock() - begin;

	_pool.try_execute( [&, cpuImg] () {
		do_decode(cpuImg);
	});
}

inline bool MultiThreadedDecoder::add(cv::Mat mat, int index)
{
	// runs once per frame
	// but the frame we attempt to decode will be an older one...
	return _gpuPool.try_execute( [&, mat, index] () {

		// scan, and fill extract ring
		std::vector<Anchor> anchors = do_scan(mat);
		bool success = anchors.size() >= 4;
		if (success)
		{
			++gpup;
			clock_t begin = clock();
			cv::UMat gpuImg = mat.getUMat(cv::ACCESS_RW);
			gpuToTicks += clock() - begin;

			_inRing[_ii] = {gpuImg, anchors};
		}
		else
			_inRing[_ii] = {};
		if (++_ii >= _inRing.size())
			_ii = 0;

		// look at extract ring, and pull the next element
		auto [gpuImg, ancr] = _inRing[_ii];
		if (gpuImg.cols > 0 and gpuImg.rows > 0)
			gpu_extract(gpuImg, ancr, index);

		// decode half
		int decodeId = index + _numThreads + 5;
		if (decodeId >= _decoderRing.size())
			decodeId -= _decoderRing.size();

		gpu_schedule_decode(decodeId);

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
	_gpuPool.stop();
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
