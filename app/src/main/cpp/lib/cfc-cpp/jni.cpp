#include "MultiThreadedDecoder.h"
#include "Processing.h"
#include "cimb_translator/CimbDecoder.h"
#include "cimb_translator/CimbReader.h"
#include "encoder/Decoder.h"
#include "extractor/Scanner.h"
#include "extractor/SimpleCameraCalibration.h"
#include "extractor/Undistort.h"

#include <jni.h>
#include <android/log.h>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <memory>
#include <sstream>

#define TAG "CameraFileCopyCPP"

using namespace std;
using namespace cv;

namespace {
	std::shared_ptr<Undistort<SimpleCameraCalibration>> _und;
	std::shared_ptr<MultiThreadedDecoder> _proc;

	unsigned _calls = 0;
	//unsigned _successfulScans = 0;
	unsigned long long _undistortTicks = 0;
	//unsigned long long _scanTicks = 0;
	//unsigned long long _extractTicks = 0;

	std::string _lastReport;
	unsigned long long _ignoreUntilFrame = 0;
	unsigned long long _lastDecodedBytes = 0;
	unsigned long long _lastDecodedFrames = 0;

	void updateUndistortTracker(const MultiThreadedDecoder& decoder)
	{
		_lastDecodedBytes = decoder.bytes;
		_lastDecodedFrames = decoder.decoded;
	}

	unsigned no0(unsigned num)
	{
		if (!num)
			return 1;
		return num;
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
	if (!_und)
		_und = std::make_shared< Undistort<SimpleCameraCalibration> >();

	// could move undistort after the first scanner check... we'll see
	_und->undistort(mat, mat);
	_undistortTicks += (clock() - begin);

	_proc->add(mat.clone());

	/*
	Scanner scanner(mat);
	std::vector<Anchor> anchors = scanner.scan();
	if (anchors.size() >= 4)
	{
		++_successfulScans;
		_scanTicks += (clock() - begin);

		Corners corners(anchors);
		Deskewer de;
		cv::Mat img = de.deskew(mat, corners);
		_extractTicks += (clock() - begin);

		cv::rotate(img, img, cv::ROTATE_90_CLOCKWISE);
		cv::cvtColor(img, img, COLOR_RGB2BGR); // opencv JavaCameraView shenanigans defeated?

		// if extracted image is small, we'll need to run some filters on it
		bool shouldPreprocess = !corners.is_granular_scale(de.total_size());
		_proc->add(img, shouldPreprocess);
	}*/

	/*if (_calls % 10 == 0 and anchors.size() == 4)
	{
		std::stringstream fname;
		fname << dataPath << "/myimage" << _calls << ".png";
		_proc->save(fname.str(), mat.clone());
	}*/

	unsigned scanFrame = _proc->scanned;
	if (scanFrame == _ignoreUntilFrame)
		updateUndistortTracker(*_proc);

	if (_calls % 60 == 0)
	{
		// check MultiThreadedDecoder metrics, only reset if we're failing a lot
		// for the first pass, use bytes / decoded, and checkpoint???
		unsigned long long currentBytes = _proc->bytes;
		unsigned long long currentDec = _proc->decoded;

		unsigned long long frames = currentDec - _lastDecodedFrames;
		if (frames != 0)
		{
			unsigned long long perFrame = (currentBytes - _lastDecodedBytes) / frames;
			if (perFrame < 4000)
				_und->reset_distortion_params();

			std::stringstream rep;
			rep << ", Per frame: " << perFrame;
			_lastReport = rep.str();
		}

		_ignoreUntilFrame = _proc->scanned + _proc->backlog();
		updateUndistortTracker(*_proc);
	}

	std::stringstream sstop;
	sstop << "MTD says: " << _proc->num_threads() << " thread(s), " << MultiThreadedDecoder::bytes << _lastReport;
	std::stringstream ssmid;
	ssmid << "#: " << MultiThreadedDecoder::decoded << " / " << MultiThreadedDecoder::scanned << " / " << _calls;
	ssmid << ". undistort: " << (_undistortTicks/no0(_calls)) << ", extract: " << (MultiThreadedDecoder::extractTicks / no0(MultiThreadedDecoder::decoded));
	ssmid << ", decode: " << (MultiThreadedDecoder::decodeTicks / no0(MultiThreadedDecoder::decoded));
	std::stringstream ssbot;
	ssbot << "Files received: " << _proc->files_decoded() << ", in flight: " << _proc->files_in_flight();

	cv::putText(mat, sstop.str(), cv::Point(5,50), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255,255,80), 2);
	cv::putText(mat, ssmid.str(), cv::Point(5,100), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255,255,80), 2);
	cv::putText(mat, ssbot.str(), cv::Point(5,150), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255,255,80), 2);

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
