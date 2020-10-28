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
	inline static clock_t extractTicks = 0;

	inline static clock_t gpud = 0;
	inline static clock_t gpuToTicks = 0;
	inline static clock_t gpuFromTicks = 0;
	inline static clock_t gpuPreTicks = 0;

	inline static std::array<umat_pair, 3> _inRing;
	inline static unsigned _ii = 0;

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
	int do_extract(const cv::UMat& mat, const cv::UMat& filtered, cv::UMat& out);
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

inline int MultiThreadedDecoder::do_extract(const cv::UMat& mat, const cv::UMat& filtered, cv::UMat& out)
{
	clock_t begin = clock();

	Scanner scanner(filtered);
	std::vector<Anchor> anchors = scanner.scan();
	++scanned;
	scanTicks += (clock() - begin);

	//if (anchors.size() >= 3) save(mat);

	if (anchors.size() < 4)
		return Extractor::FAILURE;

	begin = clock();
	Corners corners(anchors);
	Deskewer de;
	out = de.deskew(mat, corners);
	extractTicks += (clock() - begin);

	return Extractor::SUCCESS;
}

inline bool MultiThreadedDecoder::add(cv::Mat mat)
{
	return _pool.try_execute( [&, mat] () {

		++gpud;
		clock_t begin = clock();
		cv::UMat gpuImg = mat.getUMat(cv::ACCESS_RW);
		gpuToTicks += clock() - begin;

		begin = clock();
		cv::UMat gpuFilt;
		Scanner::preprocess_image(gpuImg, gpuFilt);
		gpuPreTicks += clock() - begin;

		_inRing[_ii] = {gpuImg, gpuFilt};
		if (++_ii >= _inRing.size())
			_ii = 0;

		auto [gi, gf] = _inRing[_ii];
		if (gi.cols <= 0 or gi.rows <= 0)
			return;

		cv::UMat img;
		int res = do_extract(gi, gf, img);
		if (res == Extractor::FAILURE)
			return;

		_decoderRing[_di] = img;
		if (++_di >= _decoderRing.size())
			_di = 0;

		cv::UMat& current = _decoderRing[_di];
		if (current.cols <= 0 or current.rows <= 0)
			return;

		begin = clock();
		cv::Mat temp = current.getMat(cv::ACCESS_FAST);
		gpuFromTicks += clock() - begin;

		// if extracted image is small, we'll need to run some filters on it
		begin = clock();
		bool should_preprocess = (res == Extractor::NEEDS_SHARPEN);
		unsigned decodeRes = _dec.decode_fountain(temp, _writer, should_preprocess);
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
