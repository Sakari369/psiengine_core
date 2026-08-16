#include "PSIAudio.h"

#define OFFSET_PTR(p, offset) (((ma_uint8*)(p)) + (offset))

bool PSIAudio::m_loop = false;

// Audio data callback for reading more data into audio decoder.
//
// This must always return. It runs on miniaudio's audio thread, and
// ma_device_uninit() blocks until that thread is done -- so a callback that
// does not return is a process that does not exit. Both loops below therefore
// have a termination guarantee that does not depend on the decoder.
//
// It used to have neither. ma_decoder_read_pcm_frames returns a *ma_result* in
// miniaudio 0.11 and reports the frame count through its fourth argument, which
// this passed as NULL; the return was assigned to `decoded_pcm_frame_count` and
// subtracted from the frames remaining. MA_SUCCESS is 0, so on every successful
// read the counter did not move and the loop spun forever, on the first
// callback, writing the same block of the output buffer over and over. That is
// why audio.lua hung on teardown and why it never played more than the opening
// fraction of a second.
static void ma_audio_data_callback(ma_device *device, void *output, const void *input, ma_uint32 framecount) {
	ma_decoder *decoder = (ma_decoder *)device->pUserData;
	if (decoder == NULL) {
		return;
	}

	const ma_uint32 bytes_per_frame = ma_get_bytes_per_frame(device->playback.format, device->playback.channels);

	ma_uint64 pcm_frames_remaining = framecount;
	while (pcm_frames_remaining > 0) {
		void *running_output = OFFSET_PTR(output, (framecount - pcm_frames_remaining) * bytes_per_frame);

		ma_uint64 frames_read = 0;
		ma_result result = ma_decoder_read_pcm_frames(decoder, running_output, pcm_frames_remaining, &frames_read);

		pcm_frames_remaining -= frames_read;

		if (pcm_frames_remaining == 0) {
			break;
		}

		// Short read: either the end of the file or a decode error.
		if (result != MA_SUCCESS && result != MA_AT_END) {
			break;
		}
		if (PSIAudio::m_loop == false) {
			break;
		}
		if (ma_decoder_seek_to_pcm_frame(decoder, 0) != MA_SUCCESS) {
			break;
		}
		// A file that decodes zero frames from its own start would otherwise
		// seek and retry forever.
		if (frames_read == 0) {
			break;
		}
	}

	// Whatever is left is silence rather than the previous callback's contents.
	if (pcm_frames_remaining > 0) {
		ma_silence_pcm_frames(OFFSET_PTR(output, (framecount - pcm_frames_remaining) * bytes_per_frame),
		                      pcm_frames_remaining, device->playback.format, device->playback.channels);
	}

	(void)input;
}

bool PSIAudio::init() {
	// Initialize audio context.
	//
	// Both failures below used to log and carry on, which left the code reading
	// _playback_infos[...] out of an uninitialised context and handing the
	// garbage to ma_device_init.
	if (ma_context_init(NULL, 0, NULL, &_context) != MA_SUCCESS) {
		psilog(PSILog::AUDIO, "Audio context failed initializing");
		return false;
	}
	_context_ready = true;

	// Get playback information from context.
	ma_uint32 playback_count = 0;
	ma_device_info *capture_infos = nullptr;
	ma_uint32 capture_count = 0;
	if (ma_context_get_devices(&_context, &_playback_infos, &playback_count, &capture_infos, &capture_count) != MA_SUCCESS) {
		psilog(PSILog::AUDIO, "Audio get devices failed");
		return false;
	}

	if (playback_count == 0) {
		psilog(PSILog::AUDIO, "No audio playback devices");
		return false;
	}
	if (_selected_device_index < 0 || (ma_uint32)_selected_device_index >= playback_count) {
		psilog(PSILog::AUDIO, "Audio device %d out of range (%u available); using 0",
			_selected_device_index, playback_count);
		_selected_device_index = 0;
	}

	// Print available audio devices.
	psilog(PSILog::AUDIO, "Available audio devices:");
	for (ma_uint32 device = 0; device < playback_count; ++device) {
		psilog(PSILog::AUDIO, "Audio device %d = %s", device, _playback_infos[device].name);
	}

	// Create device config.
	_config = ma_device_config_init(ma_device_type_playback);
	// Set playback device to selected id.
	_config.playback.pDeviceID = &_playback_infos[_selected_device_index].id;

	psilog(PSILog::AUDIO, "Audio initialized with device '%s'", _playback_infos[_selected_device_index].name);

	return true;
}

bool PSIAudio::play() {
	// A return rather than an assert: NDEBUG compiles the assert out, and what
	// followed it read an uninitialised decoder.
	if (_file_loaded == false) {
		psilog(PSILog::AUDIO, "Cannot play: no audio file loaded");
		return false;
	}
	if (_context_ready == false) {
		psilog(PSILog::AUDIO, "Cannot play: audio was never initialized");
		return false;
	}

	// Create configuration to match decoder.
	_config.playback.format = _decoder.outputFormat;
	_config.playback.channels = _decoder.outputChannels;
	_config.sampleRate = _decoder.outputSampleRate;

	// Set data read callback.
	_config.dataCallback = ma_audio_data_callback;
	_config.pUserData = &_decoder;

	// Initialize device with config.
	if (ma_device_init(NULL, &_config, &_device) != MA_SUCCESS) {
		psilog(PSILog::AUDIO, "Failed initializing audio device");
		return false; // Failed to initialize the device.
	}

	_device_ready = true;

	// Start playback.
	if (ma_device_start(&_device) != MA_SUCCESS) {
		psilog(PSILog::AUDIO, "Failed starting audio playback");
		return false;
	}
	_playing = true;

	psilog(PSILog::AUDIO, "Audio playback started");

	return true;
}

bool PSIAudio::load_file(std::string path) {
	ma_result result = ma_decoder_init_file(path.c_str(), NULL, &_decoder);
	if (result != MA_SUCCESS) {
		psilog(PSILog::AUDIO, "Failed creating sound from path %s", path.c_str());
		return false; // An error occurred.
	}

	psilog(PSILog::AUDIO, "Loaded audio file from %s", path.c_str());
	_file_loaded = true;
	
	return true;
}

// Release everything that was actually acquired, in reverse order, once.
//
// Every uninit here used to run unconditionally, so a script that called stop()
// without having played -- or twice -- handed miniaudio uninitialised or
// already-freed structs.
//
// The device goes first: ma_device_uninit stops the audio thread, and the
// decoder it reads from must outlive it.
bool PSIAudio::stop() {
	if (_device_ready == true) {
		ma_device_uninit(&_device);
		_device_ready = false;
		_playing = false;
	}
	if (_file_loaded == true) {
		ma_decoder_uninit(&_decoder);
		_file_loaded = false;
	}
	if (_context_ready == true) {
		ma_context_uninit(&_context);
		_context_ready = false;
	}

	return true;
}
