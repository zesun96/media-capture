/**
 * Copyright (c) 2026 sunze
 * SPDX-License-Identifier: Apache-2.0
 */

#include "media_capture/screen_source.h"

#include "ScreenCapture.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <string_view>

namespace media_capture {
namespace {

template <std::size_t Size> std::string BoundedString(const char (&value)[Size]) {
	const auto end = std::find(std::begin(value), std::end(value), '\0');
	return std::string(value, end);
}

std::uint32_t PositiveDimension(int value) {
	return value > 0 && static_cast<unsigned long long>(value) <=
	                        std::numeric_limits<std::uint32_t>::max()
	           ? static_cast<std::uint32_t>(value)
	           : 0;
}

} // namespace

std::vector<ScreenSourceInfo> EnumerateScreenSources() {
	using namespace SL::Screen_Capture;
	std::vector<ScreenSourceInfo> result;
	const auto monitors = GetMonitors();
	const auto windows = GetWindows();
	result.reserve(monitors.size() + windows.size());
	for (const auto& monitor : monitors) {
		ScreenSourceInfo source;
		source.id =
		    "monitor:" + std::to_string(Adapter(monitor)) + ":" + std::to_string(Id(monitor));
		source.label = BoundedString(monitor.Name);
		source.kind = ScreenSourceKind::Monitor;
		source.x = OffsetX(monitor);
		source.y = OffsetY(monitor);
		source.width = PositiveDimension(Width(monitor));
		source.height = PositiveDimension(Height(monitor));
		if (source.width > 0 && source.height > 0) {
			result.push_back(std::move(source));
		}
	}
	for (const auto& window : windows) {
		ScreenSourceInfo source;
		source.id = "window:" + std::to_string(window.Handle);
		source.label = BoundedString(window.Name);
		source.kind = ScreenSourceKind::Window;
		source.x = OffsetX(window);
		source.y = OffsetY(window);
		source.width = PositiveDimension(Width(window));
		source.height = PositiveDimension(Height(window));
		if (!source.label.empty() && source.width > 0 && source.height > 0) {
			result.push_back(std::move(source));
		}
	}
	return result;
}

} // namespace media_capture
