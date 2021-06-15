// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Class for calculating frametime, elapsed frames and elapsed time.

#pragma once

#include "PSIGlobals.h"
#include "PSIOpenGL.h"
#include "PSIVideo.h"
#include <time.h>

class PSIFrameTimer {
	private:
		// Time in the start of this frame.
		struct timespec _frame_start_ts;
		// Frametime stored at the end of the frame.
		GLfloat _last_frametime = 0;
		// Elapsed time in ms since beginning of this timer.
		GLfloat _elapsed_time = 0;
		// Elapsed frames since beginning of this timer.
		GLint _elapsed_frames = 0;

	public:
		PSIFrameTimer() = default;
		~PSIFrameTimer() = default;

		// Begin frame timing.
		void begin_frame();
		// End frame timing.
		GLfloat end_frame();
		// End frame timing with fixed frametime.
		GLfloat end_frame_fixed(GLfloat frametime);

		// Calculate time difference between start and end.
		inline struct timespec time_diff(timespec start, timespec end);

		void set_elapsed_time(GLfloat elapsed_time) { 
			_elapsed_time = elapsed_time;
		}
		GLfloat get_elapsed_time() {
			return _elapsed_time;
		}

		void set_elapsed_frames(GLint elapsed_frames) {
			_elapsed_frames = elapsed_frames;
		}

		GLint get_elapsed_frames() {
			return _elapsed_frames;
		}

		void reset() {
			_last_frametime = 0;
			_elapsed_time = 0;
			_elapsed_frames = 0;
		}
};
