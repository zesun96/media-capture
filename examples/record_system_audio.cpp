#include "media_capture/audio_capture.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <utility>

int main(int argc, char* argv[]) {
	media_capture::AudioCaptureConfig config;
	config.channels = 2;
	if (argc > 1) {
		config.device_id = argv[1];
	}
	std::atomic<std::uint64_t> captured_frames{0};
	std::atomic<std::uint64_t> nonzero_samples{0};
	std::atomic<std::int32_t> peak_sample{0};
	auto capture = media_capture::CreateSystemAudioCapture(
	    std::move(config), [&captured_frames, &nonzero_samples,
	                        &peak_sample](const media_capture::AudioFrameView& frame) {
		    captured_frames.fetch_add(frame.frames_per_channel);
		    std::int32_t peak = 0;
		    const auto sample_count = frame.frames_per_channel * frame.channels;
		    for (std::uint32_t index = 0; index < sample_count; ++index) {
			    const auto magnitude = std::abs(static_cast<std::int32_t>(frame.data[index]));
			    peak = std::max(peak, magnitude);
			    nonzero_samples.fetch_add(magnitude != 0 ? 1 : 0);
		    }
		    auto previous_peak = peak_sample.load();
		    while (peak > previous_peak &&
		           !peak_sample.compare_exchange_weak(previous_peak, peak)) {
		    }
	    });
	if (!capture || !capture->Start()) {
		std::cerr << "Failed to start system audio capture: "
		          << (capture ? capture->LastError() : "invalid configuration") << '\n';
		return 1;
	}
	std::cout << "Capturing system audio from " << capture->DeviceId() << " for five seconds...\n";
	std::this_thread::sleep_for(std::chrono::seconds(5));
	capture->Stop();
	std::cout << "Captured " << captured_frames.load()
	          << " system audio frames, nonzero_samples=" << nonzero_samples.load()
	          << ", peak=" << peak_sample.load() << '\n';
	return captured_frames.load() == 0 || nonzero_samples.load() == 0 ? 1 : 0;
}
