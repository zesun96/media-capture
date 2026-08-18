#include "media_capture/camera_device.h"

#include <iostream>

int main() {
	for (const auto& device : media_capture::EnumerateCameraDevices()) {
		std::cout << device.id << " " << device.label << '\n';
	}
	return 0;
}
