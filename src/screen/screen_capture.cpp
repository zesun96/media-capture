/**
 * Copyright (c) 2026 sunze
 * SPDX-License-Identifier: Apache-2.0
 */

#include "media_capture/screen_capture.h"

#include "media_capture/screen_source.h"

#include "cursor_compositor.h"

#include "ScreenCapture.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace media_capture {
namespace {

using namespace SL::Screen_Capture;

std::int64_t SteadyTimestampMicros() {
	return std::chrono::duration_cast<std::chrono::microseconds>(
	           std::chrono::steady_clock::now().time_since_epoch())
	    .count();
}

std::string MonitorId(const Monitor& monitor) {
	return "monitor:" + std::to_string(Adapter(monitor)) + ":" + std::to_string(Id(monitor));
}

std::string WindowId(const Window& window) { return "window:" + std::to_string(window.Handle); }

bool IsKnownSource(std::string_view source_id) {
	const auto sources = EnumerateScreenSources();
	return std::any_of(sources.begin(), sources.end(),
	                   [source_id](const auto& source) { return source.id == source_id; });
}

bool IsCursorVisible() noexcept {
#ifdef _WIN32
	CURSORINFO info{};
	info.cbSize = sizeof(info);
	return ::GetCursorInfo(&info) == FALSE || (info.flags & CURSOR_SHOWING) != 0;
#else
	return true;
#endif
}

class ScreenCaptureLiteCapture final : public ScreenCapture {
public:
	ScreenCaptureLiteCapture(ScreenCaptureConfig config, ScreenFrameCallback callback)
	    : config_(std::move(config)), callback_(std::move(callback)) {}

	~ScreenCaptureLiteCapture() override { Stop(); }

	bool Start() override {
		std::lock_guard<std::mutex> guard(lifecycle_mutex_);
		if (running_.load()) {
			return true;
		}
		if (!IsKnownSource(config_.source_id)) {
			SetError("screen source was not found");
			return false;
		}
		ResetCursor();
		running_.store(true);
		try {
			if (config_.source_id.starts_with("monitor:")) {
				auto capture = CreateCaptureConfiguration([source_id = config_.source_id] {
					auto monitors = GetMonitors();
					monitors.erase(std::remove_if(monitors.begin(), monitors.end(),
					                              [&](const auto& monitor) {
						                              return MonitorId(monitor) != source_id;
					                              }),
					               monitors.end());
					return monitors;
				});
				capture->onNewFrame([this](const Image& image, const Monitor& monitor) {
					OnFrame(image, OffsetX(monitor), OffsetY(monitor));
				});
				RegisterCursorCallback(capture);
				manager_ = capture->start_capturing();
			} else {
				auto capture = CreateCaptureConfiguration([source_id = config_.source_id] {
					auto windows = GetWindows();
					windows.erase(std::remove_if(windows.begin(), windows.end(),
					                             [&](const auto& window) {
						                             return WindowId(window) != source_id;
					                             }),
					              windows.end());
					return windows;
				});
				capture->onNewFrame([this](const Image& image, const Window& window) {
					OnFrame(image, OffsetX(window), OffsetY(window));
				});
				RegisterCursorCallback(capture);
				manager_ = capture->start_capturing();
			}
			if (!manager_) {
				throw std::runtime_error("screen capture manager was not created");
			}
			manager_->setFrameChangeInterval(
			    std::chrono::microseconds(1000000 / config_.frames_per_second));
			SetError({});
			return true;
		} catch (...) {
			running_.store(false);
			manager_.reset();
			SetError("failed to start screen capture");
			return false;
		}
	}

	void Stop() noexcept override {
		std::lock_guard<std::mutex> guard(lifecycle_mutex_);
		running_.store(false);
		manager_.reset();
		ResetCursor();
	}

	bool IsRunning() const noexcept override { return running_.load(); }

	std::string SourceId() const override { return config_.source_id; }

	std::string LastError() const override {
		std::lock_guard<std::mutex> guard(error_mutex_);
		return last_error_;
	}

private:
	template <typename CaptureConfiguration>
	void RegisterCursorCallback(const std::shared_ptr<CaptureConfiguration>& capture) {
		if (!config_.include_cursor) {
			return;
		}
		capture->onMouseChanged(
		    [this](const Image* image, const MousePoint& point) { OnMouseChanged(image, point); });
	}

	void OnFrame(const Image& image, int frame_x, int frame_y) noexcept {
		if (!running_.load() || image.Data == nullptr) {
			return;
		}
		try {
			const int image_width = Width(image);
			const int image_height = Height(image);
			if (image_width <= 0 || image_height <= 0 ||
			    static_cast<std::uint64_t>(image_width) >
			        std::numeric_limits<std::uint32_t>::max() / sizeof(ImageBGRA) ||
			    static_cast<std::uint64_t>(image_width) * image_height >
			        std::numeric_limits<std::size_t>::max() / sizeof(ImageBGRA)) {
				SetError("screen capture produced invalid frame dimensions");
				return;
			}
			const auto width = static_cast<std::uint32_t>(image_width);
			const auto height = static_cast<std::uint32_t>(image_height);
			std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height *
			                                 sizeof(ImageBGRA));
			Extract(image, pixels.data(), pixels.size());
			if (config_.include_cursor && IsCursorVisible()) {
				detail::CursorImage cursor;
				{
					std::lock_guard<std::mutex> guard(cursor_mutex_);
					cursor = cursor_;
				}
				if (!cursor.bgra.empty()) {
					detail::CompositeCursorBgra(
					    pixels, width, height,
					    static_cast<std::uint32_t>(width * sizeof(ImageBGRA)), frame_x, frame_y,
					    cursor);
				}
			}
			callback_({pixels.data(), width, height,
			           static_cast<std::uint32_t>(width * sizeof(ImageBGRA)),
			           SteadyTimestampMicros()});
		} catch (...) {
			SetError("screen frame callback failed");
		}
	}

	void OnMouseChanged(const Image* image, const MousePoint& point) noexcept {
		if (!running_.load()) {
			return;
		}
		try {
			detail::CursorImage updated;
			{
				std::lock_guard<std::mutex> guard(cursor_mutex_);
				updated = cursor_;
			}
			updated.position_x = X(point.Position);
			updated.position_y = Y(point.Position);
			updated.hotspot_x = X(point.HotSpot);
			updated.hotspot_y = Y(point.HotSpot);
			if (image != nullptr) {
				const int cursor_width = Width(*image);
				const int cursor_height = Height(*image);
				if (cursor_width <= 0 || cursor_height <= 0 ||
				    static_cast<std::uint64_t>(cursor_width) >
				        std::numeric_limits<std::uint32_t>::max() / sizeof(ImageBGRA) ||
				    static_cast<std::uint64_t>(cursor_width) * cursor_height >
				        std::numeric_limits<std::size_t>::max() / sizeof(ImageBGRA)) {
					SetError("screen capture produced invalid cursor dimensions");
					return;
				}
				updated.width = static_cast<std::uint32_t>(cursor_width);
				updated.height = static_cast<std::uint32_t>(cursor_height);
				updated.row_stride_bytes = updated.width * sizeof(ImageBGRA);
				updated.bgra.resize(static_cast<std::size_t>(updated.row_stride_bytes) *
				                    updated.height);
				Extract(*image, updated.bgra.data(), updated.bgra.size());
			}
			std::lock_guard<std::mutex> guard(cursor_mutex_);
			cursor_ = std::move(updated);
		} catch (...) {
			SetError("screen cursor callback failed");
		}
	}

	void ResetCursor() noexcept {
		std::lock_guard<std::mutex> guard(cursor_mutex_);
		cursor_ = {};
	}

	void SetError(std::string message) const {
		std::lock_guard<std::mutex> guard(error_mutex_);
		last_error_ = std::move(message);
	}

	ScreenCaptureConfig config_;
	ScreenFrameCallback callback_;
	std::shared_ptr<IScreenCaptureManager> manager_;
	std::atomic_bool running_{false};
	mutable std::mutex lifecycle_mutex_;
	mutable std::mutex cursor_mutex_;
	mutable std::mutex error_mutex_;
	detail::CursorImage cursor_;
	mutable std::string last_error_;
};

} // namespace

std::unique_ptr<ScreenCapture> CreateScreenCapture(ScreenCaptureConfig config,
                                                   ScreenFrameCallback callback) {
	if (config.source_id.empty() || config.frames_per_second == 0 ||
	    config.frames_per_second > 60 || !callback) {
		return nullptr;
	}
	return std::make_unique<ScreenCaptureLiteCapture>(std::move(config), std::move(callback));
}

} // namespace media_capture
