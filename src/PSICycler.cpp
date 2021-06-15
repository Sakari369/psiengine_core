#include "PSICycler.h"

GLboolean PSICycler::passed_max() {
	if ((_phase + _threshold) > _max_limit && _threshold_crossed == false) {
		_cycle_completed = true;
		_threshold_crossed = true;
		_half_cycles += 1;

		psilog(PSILog::LOGIC, "Max limit %f crossed with phase = %f (threshold %f), _half_cycles = %d", 
				     _max_limit, _phase, _threshold, _half_cycles);

		return true;
	}
	
	return false;
}

GLboolean PSICycler::passed_min() {
	if ((_phase - _threshold) < _min_limit && _threshold_crossed == true) {
		_cycle_completed = true;
		_threshold_crossed = false;
		_half_cycles += 1;

		psilog(PSILog::LOGIC, "Min limit %f crossed with phase = %f (threshold %f) _half_cycles = %d",
				     _min_limit, _phase, _threshold, _half_cycles);

		return true;
	}

	return false;
}

GLboolean PSICycler::completed_half_cycle() {
	/*
	plog("\n_phase = %.3f\n_min_limit = %.3f _max_limit = %.3f\n" \
		"%.3f + %.3f = %.3f\n" \
		"%.3f - %.3f = %.3f\n",
		_phase, _min_limit, _max_limit, 
		_phase, _threshold, _phase + _threshold,
		_phase, _threshold, _phase - _threshold);
	*/
	GLboolean passed_limits = false;
	if (passed_min() == true || passed_max() == true) {
		psilog(PSILog::MSG, "PASSED LIMITS = true");
		passed_limits = true;
	}

	return passed_limits;
}

// Reset current cycle and phase
void PSICycler::reset() {
	_phase = 0;
	_cycle_completed = true;
	_threshold_crossed = false;
	_half_cycles = 0;
}