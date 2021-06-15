#include "PSITimeDisplay.h"

// The last char is the separator
const std::string PSITimeDisplay::NUMBER_STR = "0123456789:.";
const GLint PSITimeDisplay::SEPARATOR_IDX = 10;
const GLint PSITimeDisplay::FRACTION_IDX = 11;
const GLfloat PSITimeDisplay::OPERATING_FREQ = 10.0f;

GLboolean PSITimeDisplay::init() {
	// Initialize our text renderer
	_renderer.set_text(NUMBER_STR);
	_renderer.init();

	return true;
}

#define IDX_MM 0
#define IDX_SS 1
#define IDX_MS 2

void PSITimeDisplay::logic(GLfloat frameTimeMs) {
	// Lambda function for increasing time.
	auto inc_time = [](GLint numbers[PAIR_COUNT][2], GLint idx) { 
		GLint lim_1;
		GLint lim_2;
		
		if (idx == IDX_MS) {
			lim_1 = 9;
			lim_2 = 9;
		} else {
			lim_1 = 9;
			lim_2 = 5;
		}

		numbers[idx][1] += 1;

		if (numbers[idx][1] > lim_1) {
			numbers[idx][1] = 0;

			numbers[idx][0] += 1;
			if (numbers[idx][0] > lim_2) {
				numbers[idx][0] = 0;
				return true;
			}
		}

		return false;
	};

	//plog_s("_elapsedTimeMs = %f frameTimeMs = %f", _elapsedTimeMs, frameTimeMs);

	GLfloat time_delta = frameTimeMs + _remainder;
	while (time_delta > OPERATING_FREQ) {
		time_delta = time_delta - OPERATING_FREQ;

		//plog_s("increasing, time_delta = %f counter = %d", time_delta, counter++);

		if (inc_time(_numbers, IDX_MS) == true) {
			if (inc_time(_numbers, IDX_SS) == true) {
				inc_time(_numbers, IDX_MM);
			}
		}
	}

	_remainder = time_delta;
}

void PSITimeDisplay::draw(const shared_ptr<PSIRenderContext> &ctx) {
	GLint char_idx;
	GLint sep_idx;
	GLfloat glyph_width;
	GLfloat pair_offset = 0.0f;

	glm::vec3 translation = glm::vec3(0.0f);

	//plog_s("using program %s (%d)", ctx->shader.getName().c_str(), ctx->shader.getProgram());

	// So .. we need to transform each character with glyph_width
	ctx->model.push(ctx->model.top());
	glyph_width = _renderer.get_glyph_dimensions(0).x;

	for (int pair=0; pair<PAIR_COUNT; pair++) {
		for (int i=0; i<2; i++) {
			char_idx = _numbers[pair][i];

			translation.x = pair_offset + glyph_width * i;
			ctx->model.top() = glm::translate(glm::mat4(1.0f), translation);

			_renderer.set_draw_offset(char_idx);
			_renderer.set_draw_count(1);
			_renderer.draw(ctx);
		}

		// Add one more
		pair_offset += glyph_width * 3;

		// Draw the separator
		if (pair < 2) {
			if (pair == IDX_SS) {
				sep_idx = FRACTION_IDX;
			} else {
				sep_idx = SEPARATOR_IDX;
			}

			translation.x += glyph_width;
			ctx->model.top() = glm::translate(glm::mat4(1.0f), translation);

			_renderer.set_draw_offset(sep_idx);
			_renderer.set_draw_count(1);
			_renderer.draw(ctx);
		}
	}

	ctx->model.pop();
}