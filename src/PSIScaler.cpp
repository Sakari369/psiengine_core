#include "PSIScaler.h"

void PSIScaler::calc_scale_range() {
	if (_max_val > _min_val) {
		_scale_range = _max_val - _min_val;
	} else if (_min_val > _max_val) {
		_scale_range = _min_val - _max_val;
	} else if (_max_val == _min_val) {
		_scale_range = 0.0f;
	}
}

GLfloat PSIScaler::inc_phase(GLfloat frametime) {
	_phase = _phase + _phase_dir * (TWO_PI * _freq_divided) * frametime;
	_phasetime += frametime;

	//plog_s("_phase = %f _freq_divided = %f", _phase, _freq_divided);

	if (_phase > M_PI && _half_cycle_completed == false) {
		_half_cycles++;
		_half_cycle_completed = true;
	}

	if (_phase > TWO_PI) {
		_last_phasetime = _phasetime;
		_phasetime = 0.0f;
		_phase = 0.0f;

		_half_cycles++;
		_half_cycle_completed = false;
	}

	return _phase;
}

void PSIScaler::reset() {
	_phase = 0.0f;
	_last_phasetime = 0.0f;
	_phasetime = 0.0f;
	_phase = 0.0f;
	_half_cycles = 0;
	_half_cycle_completed = false;
}
