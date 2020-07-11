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
	std::string _lastReport;

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

	/*if (_calls % 10 == 0) // and anchors.size() == 4)
	{
		std::stringstream fname;
		fname << dataPath << "/myimage" << _calls << ".png";
		_proc->save(fname.str(), img);
	}*/

	if (_lastReport.empty())
	{
		std::stringstream ss;
		cv::ocl::Context context;
		if (context.create(cv::ocl::Device::TYPE_ALL))
		{
			ss << "" << context.ndevices();
			for (int i = 0; i < context.ndevices(); i++)
				ss << ":" << context.device(i).name();

			_lastReport = ss.str();
		}
		else
			_lastReport = "fals";
	}

	std::stringstream sstop;
	sstop << "MTD says: " << _proc->num_threads() << " thread(s). " << cv::ocl::haveOpenCL() << "-" << cv::ocl::useOpenCL() << "? " << MultiThreadedDecoder::bytes << "! " << _lastReport;
	std::stringstream ssmid;
	ssmid << "#: " << MultiThreadedDecoder::perfect << " / " << MultiThreadedDecoder::decoded << " / " << MultiThreadedDecoder::scanned << " / " << _calls;
	std::stringstream ssperf;
	ssperf << "scan: " << millis(MultiThreadedDecoder::scanTicks, MultiThreadedDecoder::scanned);
	ssperf << ", extract: " << millis(MultiThreadedDecoder::extractTicks, MultiThreadedDecoder::decoded);
	ssperf << ", decode: " << millis(MultiThreadedDecoder::decodeTicks, MultiThreadedDecoder::decoded);
	std::stringstream sstats;
	sstats << "Files received: " << _proc->files_decoded() << ", in flight: " << _proc->files_in_flight();
	sstats << ". %s: " << percent(MultiThreadedDecoder::perfect, MultiThreadedDecoder::decoded) << "% : " << percent(MultiThreadedDecoder::decoded, MultiThreadedDecoder::scanned);

	cv::putText(mat, sstop.str(), cv::Point(5,50), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255,255,80), 2);
	cv::putText(mat, ssmid.str(), cv::Point(5,100), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255,255,80), 2);
	cv::putText(mat, ssperf.str(), cv::Point(5,150), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255,255,80), 2);
	cv::putText(mat, sstats.str(), cv::Point(5,200), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255,255,80), 2);
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
