#include "MultiThreadedDecoder.h"
#include "cimb_translator/CimbDecoder.h"
#include "cimb_translator/CimbReader.h"
#include "encoder/Decoder.h"
#include "extractor/Scanner.h"

#include <jni.h>
#include <android/log.h>
#include <opencv2/core/core.hpp>
#include <opencv2/core/ocl.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <memory>
#include <sstream>

#define TAG "CameraFileCopyCPP"

using namespace std;
using namespace cv;

namespace {
	std::shared_ptr<MultiThreadedDecoder> _proc;

	unsigned _calls = 0;
	bool _transferStatus = false;
	clock_t _transferSnapshotCalls = 0;

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

	void drawGuidance(cv::Mat& mat, bool in_progress)
	{
		int minsz = std::min(mat.cols, mat.rows);
		int guideWidth = minsz >> 7;
		int outlineWidth = minsz >> 8;
		int guideLength = guideWidth << 3;
		int guideOffset = minsz >> 5;

		cv::Scalar color = in_progress? cv::Scalar(0,255,0) : cv::Scalar(255,255,255);
		cv::Scalar outline = cv::Scalar(0,0,0);

		int xextra = 0;
		if (mat.cols > mat.rows)
			xextra = (mat.cols - mat.rows) >> 1;
		int yextra = 0;
		if (mat.rows > mat.cols)
			yextra = (mat.rows - mat.cols) >> 1;

		int tlx = guideOffset + xextra;
		int tly = guideOffset + yextra;
		int outlinex = tlx - (outlineWidth >> 1);
		int outliney = tly - (outlineWidth >> 1);
		cv::line(mat, cv::Point(tlx, outliney), cv::Point(tlx + guideLength, outliney), outline, outlineWidth + guideWidth);
		cv::line(mat, cv::Point(outlinex, guideOffset), cv::Point(outlinex, tly + guideLength), outline, outlineWidth + guideWidth);
		cv::line(mat, cv::Point(tlx, guideOffset), cv::Point(tlx + guideLength, tly), color, guideWidth);
		cv::line(mat, cv::Point(tlx, guideOffset), cv::Point(tlx, tly + guideLength), color, guideWidth);

		int brx = mat.cols - guideOffset - guideWidth - xextra;
		int bry = mat.rows - guideOffset - guideWidth - yextra;
		outlinex = brx - (outlineWidth >> 1);
		outliney = bry - (outlineWidth >> 1);
		cv::line(mat, cv::Point(brx, outliney), cv::Point(brx - guideLength, outliney), outline, outlineWidth + guideWidth);
		cv::line(mat, cv::Point(outlinex, bry), cv::Point(outlinex, bry - guideLength), outline, outlineWidth + guideWidth);
		cv::line(mat, cv::Point(brx, bry), cv::Point(brx - guideLength, bry), color, guideWidth);
		cv::line(mat, cv::Point(brx, bry), cv::Point(brx, bry - guideLength), color, guideWidth);
	}

	void drawDebugInfo(cv::Mat& mat)
	{
		std::stringstream sstop;
		sstop << "cfc using " << _proc->num_threads() << " thread(s). " << _proc->backlog() << "? ";
		sstop << (MultiThreadedDecoder::bytes / std::max<double>(1, MultiThreadedDecoder::decoded)) << "b v0.5a";
		std::stringstream ssmid;
		ssmid << "#: " << MultiThreadedDecoder::perfect << " / " << MultiThreadedDecoder::decoded << " / " << MultiThreadedDecoder::scanned << " / " << _calls;
		std::stringstream ssperf;
		ssperf << "scan: " << millis(MultiThreadedDecoder::scanTicks, MultiThreadedDecoder::scanned);
		ssperf << ", extract: " << millis(MultiThreadedDecoder::extractTicks, MultiThreadedDecoder::decoded);
		ssperf << ", decode: " << millis(MultiThreadedDecoder::decodeTicks, MultiThreadedDecoder::decoded);
		std::stringstream sstats;
		sstats << "Files received: " << _proc->files_decoded() << ", in flight: " << _proc->files_in_flight() << ". ";
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
}

extern "C" {
void JNICALL
Java_com_galacticicecube_camerafilecopy_MainActivity_processImageJNI(JNIEnv *env, jobject instance, jlong matAddr, jstring dataPathObj)
{
	++_calls;

	// get Mat and path from raw address
	Mat &mat = *(Mat *) matAddr;
	string dataPath(env->GetStringUTFChars(dataPathObj, NULL));

	clock_t begin = clock();

	if (!_proc)
		_proc = std::make_shared<MultiThreadedDecoder>(dataPath);

	cv::Mat img = mat.clone();
	_proc->add(img);

	if (_calls & 32)
	{
		clock_t nextSnapshot = _proc->decoded;
		_transferStatus = nextSnapshot > _transferSnapshotCalls; // decode in progress!
		_transferSnapshotCalls = nextSnapshot;
	}

	drawGuidance(mat, _transferStatus);
	drawDebugInfo(mat);

	// log computation time to Android Logcat
	double totalTime = double(clock() - begin) / CLOCKS_PER_SEC;
	__android_log_print(ANDROID_LOG_INFO, TAG, "processImage computation time = %f seconds\n",
						totalTime);
}

void JNICALL
Java_com_galacticicecube_camerafilecopy_MainActivity_shutdownJNI(JNIEnv *env, jobject instance) {
	__android_log_print(ANDROID_LOG_INFO, TAG, "Shutdown cfc-cpp\n");
	if (_proc)
		_proc->stop();
	_proc = nullptr;
}

}
