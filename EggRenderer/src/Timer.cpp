#include "api/Timer.h"
#include <stdexcept>

namespace egg
{
	Timer::Timer()
	{
		Reset();
	}

	std::float_t Timer::Measure(const TimeUnit a_Format) const
	{
		const auto current = std::chrono::high_resolution_clock::now();
		const auto difference = static_cast<std::float_t>(std::chrono::duration_cast<std::chrono::microseconds>(current - m_Begin).count());

		std::float_t formatted = 0;

		switch (a_Format)
		{
		case TimeUnit::SECONDS:
			formatted = difference / 1000000.f;
			break;
		case TimeUnit::MILLIS:
			formatted = difference / 1000.f;
			break;
		case TimeUnit::MICROS:
			formatted = difference;
			break;
		default:
			throw std::runtime_error("Trying to read timer with unimplemented format.");
			break;
		}

		return formatted;
	}

	void Timer::Reset()
	{
		m_Begin = std::chrono::high_resolution_clock::now();
	}
}
