#include "media_capture/camera_capture.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <utility>

int main(int argc, char** argv) {
	media_capture::CameraCaptureConfig config;
	if (argc > 1) {
		config.device_id = argv[1];
	}
	std::atomic_uint32_t frames = 0;
	auto capture = media_capture::CreateCameraCapture(
	    std::move(config), [&frames](const media_capture::CameraFrameView& frame) {
		    if (++frames == 1) {
			    std::cout << frame.width << 'x' << frame.height << " BGRA frame\n";
		    }
	    });
	if (!capture || !capture->Start()) {
		std::cerr << "Failed to start camera capture\n";
		return 1;
	}
	std::this_thread::sleep_for(std::chrono::seconds(3));
	capture->Stop();
	std::cout << "Captured " << frames.load() << " frames from " << capture->DeviceId() << '\n';
	return frames.load() > 0 ? 0 : 1;
}
