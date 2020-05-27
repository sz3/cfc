#pragma once

#include "fountain/fountain_decoder_sink.h"

#include "concurrentqueue/concurrentqueue.h"
#include <mutex>

template <unsigned _bufferSize>
class concurrent_fountain_decoder_sink
{
public:
	concurrent_fountain_decoder_sink(std::string data_dir)
		: _decoder(data_dir)
	{
	}

	void process()
	{
		if (_mutex.try_lock())
		{
			std::string buff;
			while (_backlog.try_dequeue(buff))
				_decoder << buff;
			_mutex.unlock();
		}
	}

	concurrent_fountain_decoder_sink& operator<<(const std::string& buffer)
	{
		_backlog.enqueue(buffer);
		process();
		return *this;
	}

protected:
	std::mutex _mutex;
	fountain_decoder_sink<_bufferSize> _decoder;
	moodycamel::ConcurrentQueue< std::string > _backlog;
};
