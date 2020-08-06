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
	clock_t _fileTransferStart = 0;
	clock_t _fileTransferTime = 0;

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

	// undistort neeeds to be on the bg thread?
	cv::Mat img = mat.clone();
	//_und->undistort(img, img);
	//_undistortTicks += (clock() - begin);

	_proc->add(img);

	if (_proc->files_in_flight() > 0 and _fileTransferStart == 0)
		_fileTransferStart = clock();
	if (_proc->files_decoded() > 0 and _fileTransferTime == 0)
		_fileTransferTime = clock() - _fileTransferStart;


	std::stringstream sstop;
	sstop << "MTD says: " << _proc->num_threads() << " thread(s). " << _proc->backlog() << "? " << cv::ocl::haveOpenCL() << "-" << cv::ocl::useOpenCL();
	sstop << "? " << (MultiThreadedDecoder::bytes / std::max<double>(1, MultiThreadedDecoder::decoded)) << "b 2ecc30.4";
	std::stringstream ssmid;
	ssmid << "#: " << MultiThreadedDecoder::perfect << " / " << MultiThreadedDecoder::decoded << " / " << MultiThreadedDecoder::scanned << " / " << _calls;
	std::stringstream ssperf;
	ssperf << "scan: " << millis(MultiThreadedDecoder::scanTicks, MultiThreadedDecoder::scanned);
	ssperf << ", extract: " << millis(MultiThreadedDecoder::extractTicks, MultiThreadedDecoder::decoded);
	ssperf << ", decode: " << millis(MultiThreadedDecoder::decodeTicks, MultiThreadedDecoder::decoded);
	std::stringstream sstats;
	sstats << "Files received: " << _proc->files_decoded() << ", in flight: " << _proc->files_in_flight();
	sstats << ". %s: " << percent(MultiThreadedDecoder::perfect, MultiThreadedDecoder::decoded) << "% : " << percent(MultiThreadedDecoder::decoded, MultiThreadedDecoder::scanned);
	std::stringstream sstats2;
	sstats2 << "File transfer time: " << millis(_fileTransferTime, 1000);

	cv::putText(mat, sstop.str(), cv::Point(5,50), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255,255,80), 2);
	cv::putText(mat, ssmid.str(), cv::Point(5,100), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255,255,80), 2);
	cv::putText(mat, ssperf.str(), cv::Point(5,150), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255,255,80), 2);
	cv::putText(mat, sstats.str(), cv::Point(5,200), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255,255,80), 2);
	cv::putText(mat, sstats2.str(), cv::Point(5,250), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255,255,80), 2);

	std::stringstream ssperf2;
	ssperf2 << "reader ctor: " << millis(Decoder::readerInitTicks, MultiThreadedDecoder::decoded);
	ssperf2 << ", fount: " << millis(Decoder::fountTicks, MultiThreadedDecoder::decoded);
	ssperf2 << ", dodecode: " << millis(Decoder::decodeTicks, MultiThreadedDecoder::decoded);
	ssperf2 << ", readloop: " << millis(Decoder::bbTicks, MultiThreadedDecoder::decoded);
	ssperf2 << ", rss: " << millis(Decoder::rssTicks, MultiThreadedDecoder::decoded);
	cv::putText(mat, ssperf2.str(), cv::Point(5,300), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255,255,80), 2);
	//*/

	/*for (const Anchor& anchor : anchors)
		cv::rectangle(mat, cv::Point(anchor.x(), anchor.y()), cv::Point(anchor.xmax(), anchor.ymax()), cv::Scalar(255,20,20), 10);*/

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
