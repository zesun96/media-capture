#include "media_capture/audio_capture.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <utility>

int main(int argc, char* argv[]) {
	media_capture::AudioCaptureConfig config;
	if (argc > 1) {
		const std::string first_argument = argv[1];
		if (first_argument.starts_with("miniaudio:")) {
			config.device_id = first_argument;
		} else {
			config.sample_rate = static_cast<std::uint32_t>(std::stoul(first_argument));
		}
	}
	if (argc > 2) {
		config.sample_rate = static_cast<std::uint32_t>(std::stoul(argv[2]));
	}
	std::atomic<std::uint64_t> captured_frames{0};
	auto capture = media_capture::CreateAudioCapture(
	    std::move(config), [&captured_frames](const media_capture::AudioFrameView& frame) {
		    captured_frames.fetch_add(frame.frames_per_channel);
	    });
	if (!capture || !capture->Start()) {
		std::cerr << "Failed to start audio capture: "
		          << (capture ? capture->LastError() : "invalid configuration") << '\n';
		return 1;
	}
	std::cout << "Capturing " << capture->DeviceId() << " for five seconds...\n";
	std::this_thread::sleep_for(std::chrono::seconds(5));
	capture->Stop();
	std::cout << "Captured " << captured_frames.load() << " frames\n";
	return captured_frames.load() == 0 ? 1 : 0;
}
