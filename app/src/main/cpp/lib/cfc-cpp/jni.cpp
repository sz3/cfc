#include "MultiThreadedDecoder.h"
#include "cimb_translator/CimbDecoder.h"
#include "cimb_translator/CimbReader.h"
#include "encoder/Decoder.h"
#include "extractor/Scanner.h"

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
	std::shared_ptr<MultiThreadedDecoder> _proc;

	unsigned _calls = 0;
	unsigned _successfulScans = 0;
	unsigned long long _scanTicks = 0;
	unsigned long long _extractTicks = 0;
	unsigned long long _decodeTicks = 0;
}

extern "C" {
void JNICALL
Java_com_galacticicecube_camerafilecopy_MainActivity_processImageJNI(JNIEnv *env, jobject instance, jlong matAddr) {

	++_calls;

	// get Mat from raw address
	Mat &mat = *(Mat *) matAddr;

	clock_t begin = clock();

	if (!_proc)
		_proc = std::make_shared<MultiThreadedDecoder>();

	Scanner scanner(mat);
	std::vector<Anchor> anchors = scanner.scan();
	if (anchors.size())
		_proc->add(mat);

	for (const Anchor& anchor : anchors) {
		cv::rectangle(mat, cv::Point(anchor.x(), anchor.y()), cv::Point(anchor.xmax(), anchor.ymax()), cv::Scalar(255,20,20), 10);
	}

	if (_calls % 10 == 0 and anchors.size() > 0 and anchors.size() < 4)
	{
		std::stringstream fname;
		fname << "/storage/emulated/0/Android/data/com.galacticicecube.camerafilecopy/files/myimage" << _calls << ".png";
		//cv::imwrite(fname.str(), mat);
	}

	std::stringstream ssmid;
	ssmid << "#: " << _successfulScans << " / " << _calls << ". scan: " << _scanTicks << ", extract: " << _extractTicks;
	std::stringstream sslow;
	sslow << "decode: " << _decodeTicks << ", idecode: " << Decoder::getTicks() << ", bytes: " << Decoder::getBytesWritten();
	std::stringstream sreader;
	sreader << "reader A: " << CimbReader::getTicksA() << ", B: " << CimbReader::getTicksB() << ", C: " << CimbReader::getTicksC() << ", decA: " << CimbDecoder::decodeTicksA;
	std::stringstream sslower;
	sslower << "dsym: " << CimbDecoder::decodeSymbolTicks() << ", bs: " << CimbDecoder::bestSymbolTicks() << ", ahash: " << CimbDecoder::ahashTicks() << ", hashe: " << CimbDecoder::extractAhashTicks();
	std::stringstream ssbot;
	ssbot << "dcolor: " << CimbDecoder::decodeColorTicks() << ", bc: " << CimbDecoder::bestColorTicks() << ", avgcolor: " << CimbDecoder::avgColorTicks();

	cv::putText(mat, ssmid.str(), cv::Point(5,200), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255,255,80), 2);
	cv::putText(mat, sslow.str(), cv::Point(5,250), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255,255,80), 2);
	cv::putText(mat, sreader.str(), cv::Point(5,300), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255,255,80), 2);
	cv::putText(mat, sslower.str(), cv::Point(5,350), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255,255,80), 2);
	cv::putText(mat, ssbot.str(), cv::Point(5,400), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255,255,80), 2);

	// log computation time to Android Logcat
	double totalTime = double(clock() - begin) / CLOCKS_PER_SEC;
	__android_log_print(ANDROID_LOG_INFO, TAG, "adaptiveThreshold computation time = %f seconds\n",
						totalTime);
}

void JNICALL
Java_com_galacticicecube_camerafilecopy_MainActivity_pauseJNI(JNIEnv *env, jobject instance) {
	__android_log_print(ANDROID_LOG_INFO, TAG, "Pause cfc-cpp\n");
}

void JNICALL
Java_com_galacticicecube_camerafilecopy_MainActivity_resumeJNI(JNIEnv *env, jobject instance) {
	__android_log_print(ANDROID_LOG_INFO, TAG, "Resume cfc-cpp\n");
}


void JNICALL
Java_com_galacticicecube_camerafilecopy_MainActivity_shutdownJNI(JNIEnv *env, jobject instance) {
	__android_log_print(ANDROID_LOG_INFO, TAG, "Shutdown cfc-cpp\n");
	if (_proc)
		_proc->stop();
	_proc = nullptr;
}

}
