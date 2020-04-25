#include "Processing.h"

#include <jni.h>
#include <android/log.h>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <sstream>

#define TAG "NativeLib"

using namespace std;
using namespace cv;

extern "C" {
void JNICALL
Java_com_example_camerafilecopy_MainActivity_adaptiveThresholdFromJNI(JNIEnv *env, jobject instance, jlong matAddr) {

    // get Mat from raw address
    Mat &mat = *(Mat *) matAddr;

    clock_t begin = clock();

    Processing p;
    std::vector<Anchor> anchors = p.scan(mat);
    std::stringstream ss;
    ss << "hello. " << anchors.size();

    cv::adaptiveThreshold(mat, mat, 255, ADAPTIVE_THRESH_MEAN_C, THRESH_BINARY_INV, 21, 5);
    cv::putText(mat, ss.str(), cv::Point(5,100), FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255,255,80), 2);

    // log computation time to Android Logcat
    double totalTime = double(clock() - begin) / CLOCKS_PER_SEC;
    __android_log_print(ANDROID_LOG_INFO, TAG, "adaptiveThreshold computation time = %f seconds\n",
                        totalTime);
}
}
