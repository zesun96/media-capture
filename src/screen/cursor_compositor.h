/**
 * Copyright (c) 2026 sunze
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>
#include <vector>

namespace media_capture::detail {

struct CursorImage {
	std::vector<std::uint8_t> bgra;
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	std::uint32_t row_stride_bytes = 0;
	std::int32_t position_x = 0;
	std::int32_t position_y = 0;
	std::int32_t hotspot_x = 0;
	std::int32_t hotspot_y = 0;
};

bool CompositeCursorBgra(std::vector<std::uint8_t>& frame, std::uint32_t frame_width,
                         std::uint32_t frame_height, std::uint32_t frame_row_stride_bytes,
                         std::int32_t frame_x, std::int32_t frame_y, const CursorImage& cursor);

} // namespace media_capture::detail
