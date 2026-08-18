/**
 * Copyright (c) 2026 sunze
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string>
#include <vector>

namespace media_capture {

struct CameraDeviceInfo {
	std::string id;
	std::string label;
};

std::vector<CameraDeviceInfo> EnumerateCameraDevices();

} // namespace media_capture
