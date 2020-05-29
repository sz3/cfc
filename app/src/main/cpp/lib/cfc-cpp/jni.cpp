#include "MultiThreadedDecoder.h"
#include "Processing.h"
#include "cimb_translator/CimbDecoder.h"
#include "cimb_translator/CimbReader.h"
#include "encoder/Decoder.h"

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
Java_com_galacticicecube_camerafilecopy_MainActivity_processImageJNI(JNIEnv *env, jobject instance, jlong matAddr, jstring dataPathObj)
{
	++_calls;

	// get Mat and path from raw address
	Mat &mat = *(Mat *) matAddr;
	string dataPath(env->GetStringUTFChars(dataPathObj, NULL));

	clock_t begin = clock();

	if (!_proc)
		_proc = std::make_shared<MultiThreadedDecoder>(dataPath);

	Scanner scanner(mat);
	std::vector<Anchor> anchors = scanner.scan();
	if (anchors.size() >= 4)
	{
		++_successfulScans;
		_scanTicks += (clock() - begin);

		Corners corners(anchors[0].center(), anchors[1].center(), anchors[2].center(), anchors[3].center());
		Deskewer de;
		cv::Mat img = de.deskew(mat, corners);
		_extractTicks += (clock() - begin);

		cv::cvtColor(img, img, COLOR_RGB2BGR); // android JavaCameraView shenanigans defeated?

		// TODO: use corners for this
		bool shouldPreprocess = (mat.cols < img.cols or mat.rows < img.rows);
		_proc->add(img, shouldPreprocess);
	}

	if (_calls % 10 == 0 and anchors.size() > 0 and anchors.size() < 4)
	{
		std::stringstream fname;
		fname << dataPath << "/myimage" << _calls << ".png";
		//cv::imwrite(fname.str(), mat);
	}

	std::stringstream sstop;
	sstop << "MTD says: " << _proc->num_threads() << " thread(s), " << MultiThreadedDecoder::decoded << ", " << MultiThreadedDecoder::bytes;
	std::stringstream ssmid;
	ssmid << "#: " << _successfulScans << " / " << _calls << ". scan: " << _scanTicks << ", deskew: " << _extractTicks << ", decode: " << MultiThreadedDecoder::ticks;

	cv::putText(mat, sstop.str(), cv::Point(5,50), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255,255,80), 2);
	cv::putText(mat, ssmid.str(), cv::Point(5,100), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255,255,80), 2);

	for (const Anchor& anchor : anchors)
		cv::rectangle(mat, cv::Point(anchor.x(), anchor.y()), cv::Point(anchor.xmax(), anchor.ymax()), cv::Scalar(255,20,20), 10);

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
