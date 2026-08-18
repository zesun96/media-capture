#include "media_capture/screen_source.h"

#include <iostream>

int main() {
	for (const auto& source : media_capture::EnumerateScreenSources()) {
		std::cout << (source.kind == media_capture::ScreenSourceKind::Monitor ? "monitor"
		                                                                      : "window")
		          << " " << source.id << " " << source.width << "x" << source.height << "+"
		          << source.x << "+" << source.y << " " << source.label << '\n';
	}
}
