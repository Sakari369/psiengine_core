#include "PSIFrameTimer.h"

#include <stdlib.h>

namespace {

// PSI_FIXED_FRAMETIME=<ms> forces end_frame() to behave as end_frame_fixed().
//
// Verification only. Most scripts already drive a fixed timestep, but the ones
// that call end_frame() advance their animation by real elapsed time, so the
// content at frame N differs between two runs and pixel-diffing a build against
// another proves nothing. Setting this makes every script deterministic per
// frame index. Unset (the default) is 0 and changes nothing.
float fixed_frametime_override() {
	static const float ms = []() {
		const char *env = getenv("PSI_FIXED_FRAMETIME");
		return (env != nullptr) ? (float)atof(env) : 0.0f;
	}();
	return ms;
}

} // namespace

void PSIFrameTimer::begin_frame() {
	// Store start of frame time.
	clock_gettime(CLOCK_MONOTONIC, &_frame_start_ts);
}

// End frame with a fixed frametime.
// This is used to achieve more smooth animation for cases where eg. object movement
// is not required to be tied to the exection time. Providing a fixed frametime on each frame
// results in more smoother appearance.
GLfloat PSIFrameTimer::end_frame_fixed(GLfloat frametime) {
	// Store last frame time.
	_last_frametime  = frametime;
	// Increase total elapsed time with fixed frametime.
	// In this case elapsed time will be essentially "ideal", not actual elapsed time.
	_elapsed_time    = _elapsed_time + frametime;
	_elapsed_frames  = _elapsed_frames + 1;

	return frametime;
}

// End frame and calculate actual frametime from beginning of frame to end of frame.
// Use this for cases where movement and animation and so on needs to be time dependent exactly.
GLfloat PSIFrameTimer::end_frame() {
	const GLfloat forced = fixed_frametime_override();
	if (forced > 0.0f) {
		return end_frame_fixed(forced);
	}

	// Get time now.
	struct timespec now_ts;
	clock_gettime(CLOCK_MONOTONIC, &now_ts);

	// Calc frametime in milliseconds.
	// This is the difference between the last frame timestamp.
	struct timespec frametime_ts = time_diff(_frame_start_ts, now_ts);

	// Calculate a millisecond value for the frame time.
	GLfloat frametime = (frametime_ts.tv_sec * 1000.0f) + (frametime_ts.tv_nsec / 1000000.0f);

	// Store last frame time.
	_last_frametime  = frametime;
	// Increase total elapsed time and frames.
	_elapsed_time    = _elapsed_time + frametime;
	_elapsed_frames  = _elapsed_frames + 1;

	return frametime;
}

inline struct timespec PSIFrameTimer::time_diff(timespec start, timespec end) {
	timespec temp;

	if ((end.tv_nsec - start.tv_nsec) < 0) {
		temp.tv_sec = end.tv_sec - start.tv_sec - 1;
		temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
	} else {
		temp.tv_sec = end.tv_sec - start.tv_sec;
		temp.tv_nsec = end.tv_nsec - start.tv_nsec;
	}

	return temp;
}