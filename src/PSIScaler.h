// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Scaler class for scaling a value between minimum and maximum values.

#pragma once

#include "PSIGlobals.h"
#include "PSIOpenGL.h"
#include "PSIMath.h"

class PSIScaler {
	public:
		PSIScaler() = default;
		~PSIScaler() = default;

		// Increase current phase by frametime dependent value.
		GLfloat inc_phase(GLfloat frametime);

		// Reset the scaler.
		void reset();

		// Return cosine eased value of current phase.
		GLfloat get_cosine_eased_phase() {
			GLfloat easing = ((-cos(_phase) / 2.0f) + 0.5f);
			return _min_val + (easing * _scale_range);
		}

		void set_phase(GLfloat phase) {
			_phase = phase;
		}
		GLfloat get_phase() {
			return _phase;
		}

		void set_freq(GLfloat freq) {
			_freq = freq;
			// This is the value we use for calculations. No need to re-calc on every frame.
			_freq_divided = freq / 1000.0f;
		}
		GLfloat get_freq() {
			return _freq;
		}

		void set_min_scale(GLfloat min_scale) {
			_min_val = min_scale;
			calc_scale_range();
		}
		GLfloat get_min_scale() {
			return _min_val;
		}

		void set_max_scale(GLfloat max_scale) {
			_max_val = max_scale;
			calc_scale_range();
		}
		GLfloat get_max_scale() {
			return _max_val;
		}

		void set_phase_dir(GLfloat phase_dir) {
			_phase_dir = phase_dir;
		}
		GLfloat get_phase_dir() {
			return _phase_dir;
		}
		GLfloat inv_phase_dir() {
			_phase_dir = -_phase_dir;
			return _phase_dir;
		}

		GLuint get_half_cycles() {
			return _half_cycles;
		}
		void reset_cycles() {
			_half_cycles = 0;
		}

		GLfloat get_scale_range() {
			return _scale_range;
		}

	private:
		// Calculate total scale range between minimum and maximum value.
		void calc_scale_range();

		// Current phase.
		GLfloat _phase = 0.0f;

		// Direction we are phasing towards (-1, 1).
		GLfloat _phase_dir = 1.0f;

		// Minimum and maximum values.
		GLfloat _min_val = 0.0f;
		GLfloat _max_val = 1.0f;

		// Scale frequency, number of complete cycles / second.
		GLfloat _freq = 1.0f;

		// Frequency used when calculating, adjusted to milliseconds.
		GLfloat _freq_divided = 1.0f / 1000.0f;

		// Calculated scale range between min .. max.
		GLfloat _scale_range = 1.0f;

		// Current phase time in ms.
		GLfloat _phasetime = 0.0f;
		// Previous phase time in ms.
		GLfloat _last_phasetime = 0.0f;

		// How many half cycles have been completed ?
		// Half cycle means that phase has surpassed Pi in radians.
		GLuint _half_cycles = 0;
		// Indicating whether we completed a half cycle.
		GLboolean _half_cycle_completed = false;
};
