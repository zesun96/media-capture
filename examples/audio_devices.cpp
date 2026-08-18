#include "media_capture/audio_device.h"

#include <iostream>

int main() {
	const auto devices = media_capture::EnumerateAudioDevices();
	for (const auto& device : devices) {
		std::cout << (device.kind == media_capture::AudioDeviceKind::Input ? "input" : "output")
		          << (device.is_default ? " [default] " : " ") << device.label << '\n'
		          << "  " << device.id << '\n';
	}
	return devices.empty() ? 1 : 0;
}
