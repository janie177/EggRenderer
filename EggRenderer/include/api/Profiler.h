#pragma once
#include "Timer.h"

namespace egg
{
	//Profiling enabled.
#ifdef EGG_PROFILING

	/*
	 * Usage:
	 *
	 * To start profiling, insert this in your code: PROFILING_START(identifier)
	 *
	 */
#define PROFILING_START(name) egg::Timer name##_timer;

   /*
    * Usage:
    *
    * To stop profiling, add: PROFILING_END(identifier, SECONDS/MILLIS/MICROS, "some information")
    * This will print the profiling information.
    */
#define PROFILING_END(name, timeUnit, info)													\
float name##_measured_time = name##_timer.Measure(TimeUnit::##timeUnit);					\
printf("Timings for %s. %s: %f %s.\n", #name, info, name##_measured_time, #timeUnit);		\

//Profiling disabled.
#else

#define PROFILING_START(name)
#define PROFILING_END(name, timeUnit, info)

#endif
}