// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Audio interface to fmodx.

#pragma once

#include "PSIGlobals.h"
#include <stdio.h>
#include <stdlib.h>
#include "ext/miniaudio.h"

class PSIAudio {
	private:
		// Audio device.
		ma_device _device;
		// Audio config.
		ma_device_config _config;
		// Audio context.
		ma_context _context;
		// Single file decoder.
		ma_decoder _decoder;
		// Currently loaded playback infos.
		ma_device_info *_playback_infos;

		// Currently selected audio device id.
		int _selected_device_index = 0;
		// Is audio currently playing ?
		bool _playing = false;
		// Has an audio file been loaded ?
		bool _file_loaded = false;

	public:
		PSIAudio() = default;
		~PSIAudio() = default;

		// Initialize audio config and context.
		bool init();
		// Uninitialize.
		bool stop();
		// Load audio file from path.
		bool load_file(std::string path);
		// Play loaded audio file.
		bool play();

		// Set selected device index.
		void set_selected_device_index(int selected_device_index) {
			_selected_device_index = selected_device_index;
		}

		void set_playing(bool playing) {
			_playing = playing;
		}
};