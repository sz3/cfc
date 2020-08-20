/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#pragma once

#include "CellPositions.h"
#include "CimbEncoder.h"

class CimbWriter
{
public:
	CimbWriter(bool dark=true, unsigned size=1024);

	bool write(unsigned bits);
	bool done() const;

	cv::Mat image() const;

protected:
	cv::Mat _image;
	CellPositions _positions;
	CimbEncoder _encoder;
};
