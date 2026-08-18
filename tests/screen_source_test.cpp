#include "media_capture/screen_capture.h"
#include "media_capture/screen_source.h"

#include <iostream>
#include <set>
#include <string>

namespace {

bool Check(bool condition, const char* message) {
	if (!condition) {
		std::cerr << message << '\n';
	}
	return condition;
}

} // namespace

int main() {
	if (!Check(!media_capture::CreateScreenCapture({}, {}),
	           "invalid screen capture configuration was accepted")) {
		return 1;
	}
	auto missing = media_capture::CreateScreenCapture({"monitor:missing", 15},
	                                                  [](const media_capture::ScreenFrameView&) {});
	if (!Check(missing != nullptr, "valid capture shape was rejected") ||
	    !Check(!missing->Start(), "missing screen source unexpectedly started") ||
	    !Check(!missing->LastError().empty(), "missing screen source did not report an error")) {
		return 1;
	}
	const auto sources = media_capture::EnumerateScreenSources();
	std::set<std::string> ids;
	for (const auto& source : sources) {
		if (!Check(!source.id.empty(), "source ID is empty") ||
		    !Check(source.width > 0, "source width is invalid") ||
		    !Check(source.height > 0, "source height is invalid") ||
		    !Check(ids.insert(source.id).second, "source ID is duplicated")) {
			return 1;
		}
		if (source.kind == media_capture::ScreenSourceKind::Monitor) {
			if (!Check(source.id.starts_with("monitor:"), "monitor ID has the wrong prefix")) {
				return 1;
			}
		} else {
			if (!Check(source.id.starts_with("window:"), "window ID has the wrong prefix") ||
			    !Check(!source.label.empty(), "window label is empty")) {
				return 1;
			}
		}
	}
	return 0;
}
