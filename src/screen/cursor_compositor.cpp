/**
 * Copyright (c) 2026 sunze
 * SPDX-License-Identifier: Apache-2.0
 */

#include "cursor_compositor.h"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace media_capture::detail {
namespace {

bool IsValidBuffer(const std::vector<std::uint8_t>& data, std::uint32_t width, std::uint32_t height,
                   std::uint32_t row_stride_bytes) {
	return width > 0 && height > 0 && width <= std::numeric_limits<std::uint32_t>::max() / 4U &&
	       row_stride_bytes >= width * 4U &&
	       height <= std::numeric_limits<std::size_t>::max() / row_stride_bytes &&
	       data.size() >= static_cast<std::size_t>(row_stride_bytes) * height;
}

std::uint8_t Blend(std::uint8_t foreground, std::uint8_t background, std::uint8_t alpha) {
	const auto inverse = static_cast<std::uint32_t>(255U - alpha);
	return static_cast<std::uint8_t>((static_cast<std::uint32_t>(foreground) * alpha +
	                                  static_cast<std::uint32_t>(background) * inverse + 127U) /
	                                 255U);
}

} // namespace

bool CompositeCursorBgra(std::vector<std::uint8_t>& frame, std::uint32_t frame_width,
                         std::uint32_t frame_height, std::uint32_t frame_row_stride_bytes,
                         std::int32_t frame_x, std::int32_t frame_y, const CursorImage& cursor) {
	if (!IsValidBuffer(frame, frame_width, frame_height, frame_row_stride_bytes) ||
	    !IsValidBuffer(cursor.bgra, cursor.width, cursor.height, cursor.row_stride_bytes)) {
		return false;
	}
	const auto left = static_cast<std::int64_t>(cursor.position_x) - cursor.hotspot_x - frame_x;
	const auto top = static_cast<std::int64_t>(cursor.position_y) - cursor.hotspot_y - frame_y;
	const auto frame_width_i64 = static_cast<std::int64_t>(frame_width);
	const auto frame_height_i64 = static_cast<std::int64_t>(frame_height);
	for (std::uint32_t cursor_y = 0; cursor_y < cursor.height; ++cursor_y) {
		const auto destination_y = top + cursor_y;
		if (destination_y < 0 || destination_y >= frame_height_i64) {
			continue;
		}
		for (std::uint32_t cursor_x = 0; cursor_x < cursor.width; ++cursor_x) {
			const auto destination_x = left + cursor_x;
			if (destination_x < 0 || destination_x >= frame_width_i64) {
				continue;
			}
			const auto cursor_offset =
			    static_cast<std::size_t>(cursor_y) * cursor.row_stride_bytes +
			    static_cast<std::size_t>(cursor_x) * 4U;
			const auto frame_offset =
			    static_cast<std::size_t>(destination_y) * frame_row_stride_bytes +
			    static_cast<std::size_t>(destination_x) * 4U;
			const auto alpha = cursor.bgra[cursor_offset + 3U];
			if (alpha == 0) {
				continue;
			}
			for (std::size_t channel = 0; channel < 3; ++channel) {
				frame[frame_offset + channel] = Blend(cursor.bgra[cursor_offset + channel],
				                                      frame[frame_offset + channel], alpha);
			}
		}
	}
	return true;
}

} // namespace media_capture::detail
