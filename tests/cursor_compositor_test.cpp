#include "cursor_compositor.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

bool Check(bool condition, const char* message) {
	if (!condition) {
		std::cerr << message << '\n';
	}
	return condition;
}

bool TestOpaqueCursorWithSourceOffset() {
	std::vector<std::uint8_t> frame(3U * 2U * 4U, 10U);
	media_capture::detail::CursorImage cursor;
	cursor.bgra = {1, 2, 3, 255};
	cursor.width = 1;
	cursor.height = 1;
	cursor.row_stride_bytes = 4;
	cursor.position_x = 102;
	cursor.position_y = 202;
	cursor.hotspot_x = 1;
	cursor.hotspot_y = 1;
	if (!Check(media_capture::detail::CompositeCursorBgra(frame, 3, 2, 12, 100, 200, cursor),
	           "valid cursor composition failed")) {
		return false;
	}
	const std::size_t offset = 1U * 12U + 1U * 4U;
	return Check(frame[offset] == 1 && frame[offset + 1] == 2 && frame[offset + 2] == 3,
	             "cursor was composed at the wrong source-relative position");
}

bool TestAlphaBlendAndClipping() {
	std::vector<std::uint8_t> frame(2U * 4U, 100U);
	media_capture::detail::CursorImage cursor;
	cursor.bgra = {200, 0, 100, 128, 40, 50, 60, 255};
	cursor.width = 2;
	cursor.height = 1;
	cursor.row_stride_bytes = 8;
	cursor.position_x = 0;
	if (!Check(media_capture::detail::CompositeCursorBgra(frame, 2, 1, 8, 0, 0, cursor),
	           "alpha cursor composition failed")) {
		return false;
	}
	if (!Check(frame[0] == 150 && frame[1] == 50 && frame[2] == 100,
	           "cursor alpha blending produced the wrong color")) {
		return false;
	}
	cursor.position_x = -1;
	std::fill(frame.begin(), frame.end(), 100U);
	if (!Check(media_capture::detail::CompositeCursorBgra(frame, 2, 1, 8, 0, 0, cursor),
	           "clipped cursor composition failed")) {
		return false;
	}
	return Check(frame[0] == 40 && frame[1] == 50 && frame[2] == 60,
	             "visible cursor pixel was not clipped and composed correctly");
}

bool TestRejectsInvalidBuffers() {
	std::vector<std::uint8_t> frame(4);
	media_capture::detail::CursorImage cursor;
	cursor.bgra.resize(4);
	cursor.width = 1;
	cursor.height = 1;
	cursor.row_stride_bytes = 3;
	return Check(!media_capture::detail::CompositeCursorBgra(frame, 1, 1, 4, 0, 0, cursor),
	             "invalid cursor stride was accepted");
}

} // namespace

int RunCursorCompositorTests() {
	return TestOpaqueCursorWithSourceOffset() && TestAlphaBlendAndClipping() &&
	               TestRejectsInvalidBuffers()
	           ? 0
	           : 1;
}
