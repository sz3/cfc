#include "MultiThreadedDecoder.h"
#include "cimb_translator/CimbDecoder.h"
#include "cimb_translator/CimbReader.h"
#include "encoder/Decoder.h"
#include "extractor/Scanner.h"
#include "serialize/format.h"

#include <jni.h>
#include <android/log.h>
#include <opencv2/core/core.hpp>
#include <opencv2/core/ocl.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <memory>
#include <mutex>
#include <sstream>

#define TAG "CameraFileCopyCPP"

using namespace std;
using namespace cv;

namespace {
	std::shared_ptr<MultiThreadedDecoder> _proc;
	std::mutex _mutex; // for _proc
	std::set<std::string> _completed;

	unsigned _calls = 0;
	int _transferStatus = 0;
	clock_t _frameDecodeSnapshot = 0;
	clock_t _frameSuccessSnapshot = 0;

	unsigned millis(unsigned num, unsigned denom)
	{
		if (!denom)
			denom = 1;
		return (num / denom) * 1000 / CLOCKS_PER_SEC;
	}

	unsigned percent(unsigned num, unsigned denom)
	{
		if (!denom)
			denom = 1;
		return (num * 100) / denom;
	}

	void drawGuidance(cv::Mat& mat, int in_progress)
	{
		int minsz = std::min(mat.cols, mat.rows);
		int guideWidth = minsz >> 7;
		int outlineWidth = guideWidth + (minsz >> 8);
		int guideLength = guideWidth << 3;
		int guideOffset = minsz >> 5;
		int outlineOffset = (outlineWidth - guideWidth) >> 1;

		cv::Scalar color = cv::Scalar(255,255,255);
		if (in_progress == 1)
			color = cv::Scalar(255,244,94); // 0,191,255
		else if (in_progress == 2)
			color = cv::Scalar(0,255,0);
		cv::Scalar outline = cv::Scalar(0,0,0);

		int xextra = 0;
		if (mat.cols > mat.rows)
			xextra = (mat.cols - mat.rows) >> 1;
		int yextra = 0;
		if (mat.rows > mat.cols)
			yextra = (mat.rows - mat.cols) >> 1;

		int lx = guideOffset + xextra;
		int ty = guideOffset + yextra;
		int outlinex = lx - outlineOffset;
		int outliney = ty - outlineOffset;
		cv::line(mat, cv::Point(lx, outliney), cv::Point(lx + guideLength, outliney), outline, outlineWidth);
		cv::line(mat, cv::Point(outlinex, guideOffset), cv::Point(outlinex, ty + guideLength), outline, outlineWidth);
		cv::line(mat, cv::Point(lx, guideOffset), cv::Point(lx + guideLength, ty), color, guideWidth);
		cv::line(mat, cv::Point(lx, guideOffset), cv::Point(lx, ty + guideLength), color, guideWidth);

		int rx = mat.cols - guideOffset - guideWidth - xextra;
		outlinex = rx + outlineOffset;
		outliney = ty - outlineOffset;
		cv::line(mat, cv::Point(rx, outliney), cv::Point(rx - guideLength, outliney), outline, outlineWidth);
		cv::line(mat, cv::Point(outlinex, guideOffset), cv::Point(outlinex, ty + guideLength), outline, outlineWidth);
		cv::line(mat, cv::Point(rx, guideOffset), cv::Point(rx - guideLength, ty), color, guideWidth);
		cv::line(mat, cv::Point(rx, guideOffset), cv::Point(rx, ty + guideLength), color, guideWidth);

		int by = mat.rows - guideOffset - guideWidth - yextra;
		outlinex = lx - outlineOffset;
		outliney = by + outlineOffset;
		cv::line(mat, cv::Point(lx, outliney), cv::Point(lx + guideLength, outliney), outline, outlineWidth);
		cv::line(mat, cv::Point(outlinex, by), cv::Point(outlinex, by - guideLength), outline, outlineWidth);
		cv::line(mat, cv::Point(lx, by), cv::Point(lx + guideLength, by), color, guideWidth);
		cv::line(mat, cv::Point(lx, by), cv::Point(lx, by - guideLength), color, guideWidth);
	}

	void drawProgress(cv::Mat& mat, const std::vector<double>& progress)
	{
		if (progress.empty())
			return;

		int minsz = std::min(mat.cols, mat.rows);
		int fillWidth = minsz >> 7;
		int outlineWidth = fillWidth + (minsz >> 8) + 1;

		int barLength = (minsz >> 1) + (minsz >> 2);
		int barOffsetW = (minsz - barLength) >> 3;
		int barOffsetL = (minsz - barLength) >> 1;
		int outlineOffset = (outlineWidth - fillWidth) >> 1;

		cv::Scalar color = cv::Scalar(255,255,255);
		cv::Scalar outline = cv::Scalar(0,0,0);

		int px = barOffsetW;
		int py = mat.rows - barOffsetL;
		for (double p : progress)
		{
			int fillLength = (barLength * p);
			cv::line(mat, cv::Point(px - outlineOffset, py), cv::Point(px - outlineOffset, py - barLength), outline, outlineWidth);
			cv::line(mat, cv::Point(px, py), cv::Point(px, py - fillLength), color, fillWidth);

			px += outlineWidth + outlineWidth;
		}
	}

	void drawDebugInfo(cv::Mat& mat, MultiThreadedDecoder& proc)
	{
		std::stringstream sstop;
		sstop << "cfc using " << proc.num_threads() << " thread(s). " << proc.mode() << ":" << proc.detected_mode() << "..." << proc.backlog() << "? ";
		sstop << (MultiThreadedDecoder::bytes / std::max<double>(1, MultiThreadedDecoder::decoded)) << "b v0.6.1";
		std::stringstream ssmid;
		ssmid << "#: " << MultiThreadedDecoder::perfect << " / " << MultiThreadedDecoder::decoded << " / " << MultiThreadedDecoder::scanned << " / " << _calls;
		std::stringstream ssperf;
		ssperf << "scan: " << millis(MultiThreadedDecoder::scanTicks, MultiThreadedDecoder::scanned);
		ssperf << ", extract: " << millis(MultiThreadedDecoder::extractTicks, MultiThreadedDecoder::decoded);
		ssperf << ", decode: " << millis(MultiThreadedDecoder::decodeTicks, MultiThreadedDecoder::decoded);
		std::stringstream sstats;
		sstats << "Files received: " << proc.files_decoded() << ", in flight: " << proc.files_in_flight() << ". ";
		sstats << percent(MultiThreadedDecoder::perfect, MultiThreadedDecoder::decoded) << "% decode. ";
		sstats << percent(MultiThreadedDecoder::decoded, MultiThreadedDecoder::scanned) << "% scan.";

		cv::putText(mat, sstop.str(), cv::Point(5,50), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255,255,80), 2);
		cv::putText(mat, ssmid.str(), cv::Point(5,100), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255,255,80), 2);
		cv::putText(mat, ssperf.str(), cv::Point(5,150), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255,255,80), 2);
		cv::putText(mat, sstats.str(), cv::Point(5,200), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255,255,80), 2);

		/*std::stringstream ssperf2;
		ssperf2 << "reader ctor: " << millis(Decoder::readerInitTicks, MultiThreadedDecoder::decoded);
		ssperf2 << ", fount: " << millis(Decoder::fountTicks, MultiThreadedDecoder::decoded);
		ssperf2 << ", dodecode: " << millis(Decoder::decodeTicks, MultiThreadedDecoder::decoded);
		ssperf2 << ", readloop: " << millis(Decoder::bbTicks, MultiThreadedDecoder::decoded);
		ssperf2 << ", rss: " << millis(Decoder::rssTicks, MultiThreadedDecoder::decoded);
		cv::putText(mat, ssperf2.str(), cv::Point(5,300), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255,255,80), 2);
		//*/
	}

	std::string jstring_to_cppstr(JNIEnv *env, const jstring& dataPathObj)
	{
		const char* temp = env->GetStringUTFChars(dataPathObj, NULL);
		string res(temp);
		env->ReleaseStringUTFChars(dataPathObj, temp);
		return res;
	}
}

extern "C" {
jstring JNICALL
Java_org_cimbar_camerafilecopy_MainActivity_processImageJNI(JNIEnv *env, jobject instance, jlong matAddr, jstring dataPathObj, jint modeInt)
{
	++_calls;

	// get params from raw address
	Mat &mat = *(Mat *) matAddr;
	string dataPath = jstring_to_cppstr(env, dataPathObj);
	int modeVal = (int)modeInt;

	std::shared_ptr<MultiThreadedDecoder> proc;
	{
		std::lock_guard<std::mutex> lock(_mutex);
		if (!_proc or !_proc->set_mode(modeVal))
			_proc = std::make_shared<MultiThreadedDecoder>(dataPath, modeVal);
		proc = _proc;
	}

	clock_t begin = clock();
	cv::Mat img = mat.clone();
	proc->add(img);

	if (_calls & 32)
	{
		clock_t decodeSnapshot = proc->decoded;
		clock_t perfectSnapshot = proc->perfect;
		_transferStatus = perfectSnapshot > _frameSuccessSnapshot; // a bit silly, but 1 == partial decode
		_transferStatus += (decodeSnapshot > _frameDecodeSnapshot); // 2 == full decode
		_frameDecodeSnapshot = decodeSnapshot;
		_frameSuccessSnapshot = perfectSnapshot;
	}

	drawProgress(mat, proc->get_progress());
	drawGuidance(mat, _transferStatus);
	//drawDebugInfo(mat, *proc);

	// log computation time to Android Logcat
	double totalTime = double(clock() - begin) / CLOCKS_PER_SEC;
	__android_log_print(ANDROID_LOG_INFO, TAG, "processImage computation time = %f seconds\n",
						totalTime);

	// return a decoded file to prompt the user to save it, if there is a new one
	string result;
	if (proc->detected_mode()) // repurpose str for special message passing
		result = fmt::format("/{}", proc->detected_mode());

	std::vector<string> all_decodes = proc->get_done();
	for (string& s : all_decodes)
		if (_completed.find(s) == _completed.end())
		{
			_completed.insert(s);
			result = s;
		}
	return env->NewStringUTF(result.c_str());
}

void JNICALL
Java_org_cimbar_camerafilecopy_MainActivity_shutdownJNI(JNIEnv *env, jobject instance) {
	__android_log_print(ANDROID_LOG_INFO, TAG, "Shutdown cfc-cpp\n");

	std::lock_guard<std::mutex> lock(_mutex);
	if (_proc)
		_proc->stop();
	_proc = nullptr;
}

}
