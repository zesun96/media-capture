/**
 * Copyright (c) 2026 sunze
 * SPDX-License-Identifier: Apache-2.0
 */

#include "media_capture/camera_capture.h"

#include "ccap.h"

#include <atomic>
#include <chrono>
#include <limits>
#include <mutex>
#include <utility>

namespace media_capture {
namespace {

std::int64_t SteadyTimestampMicros() {
	return std::chrono::duration_cast<std::chrono::microseconds>(
	           std::chrono::steady_clock::now().time_since_epoch())
	    .count();
}

class CameraCaptureCcap final : public CameraCapture {
public:
	CameraCaptureCcap(CameraCaptureConfig config, CameraFrameCallback callback)
	    : config_(std::move(config)), callback_(std::move(callback)) {
		ready_ = Configure(provider_, config_.device_id, device_id_);
	}

	~CameraCaptureCcap() override {
		Stop();
		std::lock_guard<std::mutex> guard(mutex_);
		provider_.setNewFrameCallback({});
		provider_.close();
	}

	bool Start() override {
		std::lock_guard<std::mutex> guard(mutex_);
		if (!ready_) {
			return false;
		}
		if (provider_.isStarted()) {
			return true;
		}
		next_frame_due_us_.store(0);
		if (!provider_.start()) {
			SetError("failed to start camera capture");
			return false;
		}
		SetError({});
		return true;
	}

	void Stop() noexcept override {
		try {
			std::lock_guard<std::mutex> guard(mutex_);
			provider_.stop();
		} catch (...) {
			SetError("failed to stop camera capture");
		}
	}

	bool IsRunning() const noexcept override {
		try {
			std::lock_guard<std::mutex> guard(mutex_);
			return provider_.isStarted();
		} catch (...) {
			return false;
		}
	}

	std::string DeviceId() const override {
		std::lock_guard<std::mutex> guard(mutex_);
		return device_id_;
	}

	bool SwitchDevice(std::string_view device_id) override {
		if (device_id.empty()) {
			return false;
		}
		std::lock_guard<std::mutex> guard(mutex_);
		if (device_id == device_id_) {
			return true;
		}
		ccap::Provider replacement;
		std::string resolved_id;
		if (!Configure(replacement, device_id, resolved_id)) {
			return false;
		}
		const bool was_running = provider_.isStarted();
		next_frame_due_us_.store(0);
		if (was_running && !replacement.start()) {
			SetError("failed to start replacement camera device");
			return false;
		}
		provider_.stop();
		provider_.setNewFrameCallback({});
		provider_.close();
		provider_ = std::move(replacement);
		device_id_ = std::move(resolved_id);
		ready_ = true;
		SetError({});
		return true;
	}

	std::string LastError() const override {
		std::lock_guard<std::mutex> guard(error_mutex_);
		return last_error_;
	}

	bool Ready() const noexcept { return ready_; }

private:
	bool Configure(ccap::Provider& provider, std::string_view requested_id,
	               std::string& resolved_id) {
		if (config_.width == 0 || config_.height == 0 || config_.frames_per_second == 0 ||
		    config_.frames_per_second > 240 ||
		    config_.width > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
		    config_.height > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
			SetError("invalid camera capture dimensions or frame rate");
			return false;
		}
		provider.set(ccap::PropertyName::Width, config_.width);
		provider.set(ccap::PropertyName::Height, config_.height);
		provider.set(ccap::PropertyName::FrameRate, config_.frames_per_second);
		provider.set(ccap::PropertyName::PixelFormatOutput, ccap::PixelFormat::BGRA32);
		provider.set(ccap::PropertyName::FrameOrientation, ccap::FrameOrientation::TopToBottom);
		provider.setNewFrameCallback([this](const std::shared_ptr<ccap::VideoFrame>& frame) {
			if (frame == nullptr || frame->pixelFormat != ccap::PixelFormat::BGRA32 ||
			    frame->data[0] == nullptr) {
				SetError("camera backend returned an unsupported frame");
				return true;
			}
			if (!ShouldDeliverFrame()) {
				return true;
			}
			try {
				callback_({frame->data[0], frame->width, frame->height, frame->stride[0],
				           static_cast<std::int64_t>(frame->timestamp / 1000U)});
			} catch (...) {
				SetError("camera frame callback threw an exception");
			}
			return true;
		});
		if (!provider.open(requested_id, false)) {
			SetError("failed to open camera device");
			return false;
		}
		const auto info = provider.getDeviceInfo();
		resolved_id = info.has_value() ? info->deviceName : std::string(requested_id);
		return true;
	}

	bool ShouldDeliverFrame() noexcept {
		const auto now = SteadyTimestampMicros();
		const auto interval = static_cast<std::int64_t>(1000000U / config_.frames_per_second);
		const auto tolerance = interval / 10;
		auto due = next_frame_due_us_.load();
		for (;;) {
			if (due != 0 && now + tolerance < due) {
				return false;
			}
			const auto next_due =
			    due == 0 || now > due + interval ? now + interval : due + interval;
			if (next_frame_due_us_.compare_exchange_weak(due, next_due)) {
				return true;
			}
		}
	}

	void SetError(std::string error) const {
		std::lock_guard<std::mutex> guard(error_mutex_);
		last_error_ = std::move(error);
	}

	mutable std::mutex mutex_;
	mutable std::mutex error_mutex_;
	CameraCaptureConfig config_;
	CameraFrameCallback callback_;
	ccap::Provider provider_;
	std::string device_id_;
	mutable std::string last_error_;
	std::atomic<std::int64_t> next_frame_due_us_{0};
	bool ready_ = false;
};

} // namespace

std::unique_ptr<CameraCapture> CreateCameraCapture(CameraCaptureConfig config,
                                                   CameraFrameCallback callback) {
	return CreateCameraCapture(std::move(config), std::move(callback), nullptr);
}

std::unique_ptr<CameraCapture>
CreateCameraCapture(CameraCaptureConfig config, CameraFrameCallback callback, std::string* error) {
	if (!callback) {
		if (error != nullptr) {
			*error = "camera frame callback is empty";
		}
		return nullptr;
	}
	auto capture = std::make_unique<CameraCaptureCcap>(std::move(config), std::move(callback));
	if (!capture->Ready()) {
		if (error != nullptr) {
			*error = capture->LastError();
		}
		return nullptr;
	}
	if (error != nullptr) {
		error->clear();
	}
	return capture;
}

} // namespace media_capture
