#include "media_capture/screen_capture.h"
#include "media_capture/screen_source.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>

int main() {
	const auto sources = media_capture::EnumerateScreenSources();
	const auto source = std::find_if(sources.begin(), sources.end(), [](const auto& candidate) {
		return candidate.kind == media_capture::ScreenSourceKind::Monitor;
	});
	if (source == sources.end()) {
		std::cerr << "No monitor is available\n";
		return 1;
	}
	std::mutex mutex;
	std::condition_variable changed;
	bool received = false;
	auto capture = media_capture::CreateScreenCapture(
	    {source->id, 15}, [&](const media_capture::ScreenFrameView& frame) {
		    std::lock_guard<std::mutex> guard(mutex);
		    if (!received) {
			    std::cout << "Captured " << frame.width << "x" << frame.height << " BGRA frame\n";
			    received = true;
			    changed.notify_one();
		    }
	    });
	if (!capture || !capture->Start()) {
		std::cerr << "Failed to start monitor capture\n";
		return 1;
	}
	{
		std::unique_lock<std::mutex> lock(mutex);
		changed.wait_for(lock, std::chrono::seconds(5), [&] { return received; });
	}
	capture->Stop();
	return received ? 0 : 1;
}
