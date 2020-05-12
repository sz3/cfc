#pragma once

#include "encoder/Decoder.h"
#include "extractor/Anchor.h"
#include "extractor/Corners.h"
#include "extractor/Deskewer.h"
#include "extractor/Scanner.h"

#include <opencv2/opencv.hpp>
#include <vector>

class Processing
{
public:
	Processing() {}

	std::vector<Anchor> scan(cv::Mat& mat)
	{
		Scanner scanner(mat);
		std::vector<Anchor> anchors = scanner.scan();

		std::stringstream sstop;
		sstop << mat.rows << " x " << mat.cols << " : " << anchors.size() << " anchors.";
		cv::putText(mat, sstop.str(), cv::Point(5,50), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255,255,80), 2);
		return anchors;
	}

	cv::Mat extract(cv::Mat& mat, const std::vector<Anchor>& points)
	{
		Corners corners(points[0].center(), points[1].center(), points[2].center(), points[3].center());
		Deskewer de;
		cv::Mat out = de.deskew(mat, corners);
		cv::rotate(out, out, cv::ROTATE_90_CLOCKWISE);

		std::stringstream sstop;
		sstop << out.rows << " x " << out.cols;
		cv::putText(mat, sstop.str(), cv::Point(5,100), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255,255,80), 2);
		return out;
	}

	unsigned decode(cv::Mat& mat, const cv::Mat& img)
	{
		Decoder de(0);
		unsigned res = de.decode(img, "/dev/shm/camerafilecopy.tmp.txt");

		std::stringstream sstop;
		sstop << "decoded " << res << " bytes.";
		cv::putText(mat, sstop.str(), cv::Point(5,150), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255,255,80), 2);
		return res;
	}

protected:
};
