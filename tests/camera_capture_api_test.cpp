#include "media_capture/camera_capture.h"
#include "media_capture/camera_device.h"

#include <algorithm>
#include <cassert>
#include <utility>

int main() {
	const auto devices = media_capture::EnumerateCameraDevices();
	assert(std::all_of(devices.begin(), devices.end(), [](const auto& device) {
		return !device.id.empty() && !device.label.empty();
	}));
	media_capture::CameraCaptureConfig config;
	config.device_id = "media-capture-device-that-does-not-exist";
	assert(media_capture::CreateCameraCapture(std::move(config), [](const auto&) {}) == nullptr);
	assert(media_capture::CreateCameraCapture({}, {}) == nullptr);
	return 0;
}
