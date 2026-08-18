#include "media_capture/audio_capture.h"

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
	if (!Check(defaults.sample_rate == 48000, "unexpected default sample rate") ||
	    !Check(defaults.channels == 1, "unexpected default channel count") ||
	    !Check(!media_capture::CreateAudioCapture({}, {}), "an empty callback must be rejected")) {
		return 1;
	}
	auto capture = media_capture::CreateAudioCapture({}, [](const auto&) {});
	return Check(capture != nullptr, "valid capture configuration was rejected") &&
	               Check(!capture->IsRunning(), "new capture unexpectedly started") &&
	               Check(!capture->SwitchDevice(""), "empty device ID was accepted")
	           ? 0
	           : 1;
}
