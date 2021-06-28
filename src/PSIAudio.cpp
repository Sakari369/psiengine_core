#include "PSIAudio.h"

#define OFFSET_PTR(p, offset) (((ma_uint8*)(p)) + (offset))

bool PSIAudio::m_loop = false;

// Audio data callback for reading more data into audio decoder.
static void ma_audio_data_callback(ma_device *device, void *output, const void *input, ma_uint32 framecount) {
	ma_decoder *decoder = (ma_decoder *)device->pUserData;
	if (decoder == NULL) {
		return;
	}

	ma_uint64 pcm_frames_remaining = framecount;
	while (pcm_frames_remaining > 0) {
		ma_uint32 bytes_per_frame = ma_get_bytes_per_frame(device->playback.format, device->playback.channels);
		void *running_output = OFFSET_PTR(output, (framecount - pcm_frames_remaining) * bytes_per_frame);
		ma_uint64 decoded_pcm_frame_count = ma_decoder_read_pcm_frames(decoder, running_output, pcm_frames_remaining);

		// Loop back to the start if we've reached the end. 
		if (PSIAudio::m_loop == true) {
			if (decoded_pcm_frame_count < pcm_frames_remaining) {
				ma_decoder_seek_to_pcm_frame(decoder, 0);
			}
		}

		pcm_frames_remaining -= decoded_pcm_frame_count;
	}

	(void)input;
}

bool PSIAudio::init() {
	// Initialize audio context.
	if (ma_context_init(NULL, 0, NULL, &_context) != MA_SUCCESS) {
		psilog(PSILog::AUDIO, "Audio context failed initializing");
	}

	// Get playback information from context.
	ma_uint32 playback_count;
	ma_device_info *capture_infos;
	ma_uint32 capture_count;
	if (ma_context_get_devices(&_context, &_playback_infos, &playback_count, &capture_infos, &capture_count) != MA_SUCCESS) {
		psilog(PSILog::AUDIO, "Audio get devices failed");
		// Error.
	}

	// Print available audio devices.
	psilog(PSILog::AUDIO, "Available audio devices:");
	for (ma_uint32 device = 0; device < playback_count; ++device) {
		psilog(PSILog::AUDIO, "Audio device %d = %s\n", device, _playback_infos[device].name);
	}

	// Create device config.
	_config = ma_device_config_init(ma_device_type_playback);
	// Set playback device to selected id.
	_config.playback.pDeviceID = &_playback_infos[_selected_device_index].id;

	psilog(PSILog::INIT, "Audio initialized with device '%s'", _playback_infos[_selected_device_index].name);

	return true;
}

bool PSIAudio::play() {
	assert(_file_loaded != false);

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

	// Start playback.
	ma_device_start(&_device);

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

bool PSIAudio::stop() {
	ma_device_uninit(&_device);
	ma_context_uninit(&_context);
	ma_decoder_uninit(&_decoder);

	return true;
}