// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Phase cycler. Keeps track of pulse sequence completed cycles and passed limits.
// Used for keeping track of how many times a pulse sequence has completed for example.

#pragma once

#include "PSIGlobals.h"
#include "PSIOpenGL.h"
#include "PSIMath.h"

class PSICycler {
	private:
		// Current phase.
		GLfloat _phase = 0.0f;

		GLfloat _min_limit = -1.0f;
		GLfloat _max_limit = 1.0f;

		// Threshold for going over a half cycle
		GLfloat _threshold = 0.05f;

		// What dir of the cycler are we progressing.
		GLint _phase_dir = 1;

		GLboolean _threshold_crossed = false;
		GLboolean _cycle_completed = false;

		GLint _half_cycles = 0;

	public:
		PSICycler() = default;
		~PSICycler() = default;

		// Has the cycler completed half of its current cycle ?
		GLboolean completed_half_cycle();
		// Has the cycler passed minimum value ?
		GLboolean passed_min();
		// Has the cycler passed maximum value ?
		GLboolean passed_max();
		// Reset cycler.
		void reset();

		// Cycler phase as frequency.
		void set_phase(GLfloat phase) {
			_phase = phase;
		}
		GLfloat get_phase() {
			return _phase;
		}

		// Direction phase is travelling.
		void set_phase_dir(GLint phase_dir) {
			_phase_dir = phase_dir;
		}
		GLint get_phase_dir() {
			return _phase_dir;
		}
		GLint inv_phase_dir() {
			_phase_dir = -_phase_dir;
			return _phase_dir;
		}

		// Upper limit for cycler phase.
		void set_max_limit(GLfloat max_limit) {
			_max_limit = max_limit;
		}
		GLfloat get_max_limit() {
			return _max_limit;
		}

		// Lower limit for cycler phase.
		void set_min_limit(GLfloat min_limit) {
			_min_limit = min_limit;
		}
		GLfloat get_min_limit() {
			return _min_limit;
		}

		// Threshold for passing lower/upper limit.
		void set_threshold(GLfloat threshold) {
			_threshold = threshold;
		}
		GLfloat get_threshold() {
			return _threshold;
		}

		// How many half cycles has the cycler completed ?
		GLint get_half_cycles() {
			return _half_cycles;
		}
};
