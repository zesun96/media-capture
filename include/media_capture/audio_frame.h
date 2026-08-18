/**
 * Copyright (c) 2026 sunze
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>

namespace media_capture {

struct AudioFrameView {
	const std::int16_t* data = nullptr;
	std::uint32_t sample_rate = 0;
	std::uint32_t channels = 0;
	std::uint32_t frames_per_channel = 0;
	std::int64_t timestamp_us = 0;
};

} // namespace media_capture
