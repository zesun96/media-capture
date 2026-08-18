/**
 * Copyright (c) 2026 sunze
 * SPDX-License-Identifier: Apache-2.0
 */

#include "media_capture/camera_device.h"

#include "ccap.h"

#include <utility>

namespace media_capture {

std::vector<CameraDeviceInfo> EnumerateCameraDevices() {
	ccap::Provider provider;
	std::vector<CameraDeviceInfo> result;
	for (auto& name : provider.findDeviceNames()) {
		result.push_back({name, std::move(name)});
	}
	return result;
}

} // namespace media_capture
