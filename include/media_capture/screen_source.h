/**
 * Copyright (c) 2026 sunze
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace media_capture {

enum class ScreenSourceKind {
	Monitor,
	Window,
};

struct ScreenSourceInfo {
	std::string id;
	std::string label;
	ScreenSourceKind kind = ScreenSourceKind::Monitor;
	std::int32_t x = 0;
	std::int32_t y = 0;
	std::uint32_t width = 0;
	std::uint32_t height = 0;
};

std::vector<ScreenSourceInfo> EnumerateScreenSources();

} // namespace media_capture
