#include "media_capture/audio_capture.h"
#include "media_capture/audio_playback.h"

#include <iostream>

namespace {

bool Check(bool condition, const char* message) {
	if (!condition) {
		std::cerr << message << '\n';
	}
	return condition;
}

} // namespace

int main() {
	media_capture::AudioCaptureConfig defaults;
	media_capture::AudioPlaybackConfig playback_defaults;
	if (!Check(defaults.sample_rate == 48000, "unexpected default sample rate") ||
	    !Check(defaults.channels == 1, "unexpected default channel count") ||
	    !Check(playback_defaults.sample_rate == 48000, "unexpected default playback sample rate") ||
	    !Check(playback_defaults.channels == 2, "unexpected default playback channel count") ||
	    !Check(playback_defaults.buffer_duration_ms == 200,
	           "unexpected default playback buffer duration") ||
	    !Check(!media_capture::CreateAudioCapture({}, {}), "an empty callback must be rejected")) {
		return 1;
	}
	if (!Check(!media_capture::CreateAudioPlayback({.sample_rate = 0}),
	           "zero playback sample rate was accepted") ||
	    !Check(!media_capture::CreateAudioPlayback({.channels = 3}),
	           "unsupported playback channel count was accepted") ||
	    !Check(!media_capture::CreateAudioPlayback({.buffer_duration_ms = 5001}),
	           "oversized playback buffer was accepted")) {
		return 1;
	}
	auto capture = media_capture::CreateAudioCapture({}, [](const auto&) {});
	media_capture::AudioCaptureConfig system_audio_config;
	system_audio_config.channels = 2;
	auto system_audio =
	    media_capture::CreateSystemAudioCapture(system_audio_config, [](const auto&) {});
	auto playback = media_capture::CreateAudioPlayback();
	return Check(capture != nullptr, "valid capture configuration was rejected") &&
	               Check(!capture->IsRunning(), "new capture unexpectedly started") &&
	               Check(!capture->SwitchDevice(""), "empty device ID was accepted") &&
	               Check(system_audio != nullptr,
	                     "valid system audio capture configuration was rejected") &&
	               Check(!system_audio->IsRunning(),
	                     "new system audio capture unexpectedly started") &&
	               Check(playback != nullptr, "valid playback configuration was rejected") &&
	               Check(!playback->IsRunning(), "new playback unexpectedly started") &&
	               Check(playback->Volume() == 1.0F, "unexpected default playback volume") &&
	               Check(playback->SetVolume(0.25F), "valid playback volume was rejected") &&
	               Check(playback->Volume() == 0.25F, "playback volume was not stored") &&
	               Check(!playback->SetVolume(-0.1F), "negative playback volume was accepted") &&
	               Check(!playback->SwitchDevice(""), "empty output device ID was accepted")
	           ? 0
	           : 1;
}
