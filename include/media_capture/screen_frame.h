/**
 * Copyright (c) 2026 sunze
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>

namespace media_capture {

struct ScreenFrameView {
	const std::uint8_t* data = nullptr;
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	std::uint32_t row_stride_bytes = 0;
	std::int64_t timestamp_us = 0;
};

} // namespace media_capture
