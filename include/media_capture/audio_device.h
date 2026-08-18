/**
 * Copyright (c) 2026 sunze
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string>
#include <vector>

namespace media_capture {

enum class AudioDeviceKind {
	Input,
	Output,
};

struct AudioDeviceInfo {
	std::string id;
	std::string label;
	AudioDeviceKind kind = AudioDeviceKind::Input;
	bool is_default = false;
};

std::vector<AudioDeviceInfo> EnumerateAudioDevices();

} // namespace media_capture
