#pragma once

#include "extractor/Anchor.h"
#include "extractor/Scanner.h"

#include <opencv2/opencv.hpp>
#include <vector>

class Processing
{
public:
    Processing() {}

    std::vector<Anchor> scan(const cv::Mat& img)
    {
        Scanner scanner(img);
        return scanner.scan();
    }
protected:
};
