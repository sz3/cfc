/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#pragma once

#include "FountainInit.h"
#include "wirehair/wirehair.h"
#include <cassert>

class FountainEncoder
{
public:
	FountainEncoder() = default;

	FountainEncoder(const uint8_t* data, size_t length, size_t packet_size)
		: _packetSize(packet_size)
	{
		FountainInit::init();
		_codec = wirehair_encoder_create(nullptr, data, length, packet_size);
	}

	~FountainEncoder()
	{
		if (_codec)
			wirehair_free(_codec);
	}

	// no copy
	FountainEncoder(const FountainEncoder&) = delete;
	FountainEncoder& operator=(const FountainEncoder&) = delete;

	FountainEncoder(FountainEncoder&& other) noexcept
		: _codec(std::exchange(other._codec, nullptr))
		, _packetSize(other._packetSize)
	{}

	FountainEncoder& operator=(FountainEncoder&& other) noexcept
	{
		if (this != &other)
		{
			if (_codec)
				wirehair_free(_codec);
			_codec = std::exchange(other._codec, nullptr);
			_packetSize = other._packetSize;
		}
		return *this;
	}

	bool good() const
	{
		return _codec != nullptr;
	}

	size_t encode(unsigned block_num, uint8_t* buff, size_t size)
	{
		assert(size == _packetSize);
		uint32_t written = 0;
		WirehairResult res = wirehair_encode(_codec, block_num, buff, size, &written);
		if (res != Wirehair_Success)
			return 0;
		return written;
	}

	size_t packet_size() const
	{
		return _packetSize;
	}

protected:
	WirehairCodec _codec = nullptr;
	size_t _packetSize;
};
