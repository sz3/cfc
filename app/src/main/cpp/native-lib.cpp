#include <jni.h>

#include "opencv2/opencv.hpp"
#include <string>
#include <sstream>


extern "C" JNIEXPORT jstring JNICALL
Java_com_example_camerafilecopy_MainActivity_stringFromJNI(
		JNIEnv* env,
		jobject /* this */) {

	std::stringstream ss;
	ss << "cpus: " << cv::getNumberOfCPUs() << " , ticks: " << cv::getTickCount();
	std::string hello = "Hello from C++! " + ss.str();
	return env->NewStringUTF(hello.c_str());
}
