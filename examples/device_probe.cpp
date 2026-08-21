/**
 * Copyright (c) 2026 sunze
 * SPDX-License-Identifier: Apache-2.0
 */

#include "media_capture/audio_capture.h"
#include "media_capture/audio_device.h"
#include "media_capture/camera_capture.h"
#include "media_capture/camera_device.h"
#include "media_capture/screen_capture.h"
#include "media_capture/screen_source.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

namespace {

using Clock = std::chrono::steady_clock;

struct MetricsSnapshot {
	std::uint64_t frames = 0;
	std::uint64_t payload_units = 0;
	std::uint64_t invalid_frames = 0;
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	double first_frame_ms = -1;
	double max_gap_ms = 0;
};

class FrameMetrics {
public:
	FrameMetrics() : started_(Clock::now()) {}

	void Observe(bool valid, std::uint64_t payload_units = 1, std::uint32_t width = 0,
	             std::uint32_t height = 0) {
		const auto now = Clock::now();
		std::lock_guard<std::mutex> guard(mutex_);
		if (!valid) {
			++invalid_frames_;
		}
		if (frames_ == 0) {
			first_frame_ms_ = Milliseconds(now - started_);
		} else {
			max_gap_ms_ = std::max(max_gap_ms_, Milliseconds(now - previous_));
		}
		previous_ = now;
		++frames_;
		payload_units_ += payload_units;
		if (width > 0 && height > 0) {
			width_ = width;
			height_ = height;
		}
	}

	MetricsSnapshot Snapshot() const {
		std::lock_guard<std::mutex> guard(mutex_);
		return {frames_, payload_units_,  invalid_frames_, width_,
		        height_, first_frame_ms_, max_gap_ms_};
	}

private:
	template <typename Duration> static double Milliseconds(Duration duration) {
		return std::chrono::duration<double, std::milli>(duration).count();
	}

	Clock::time_point started_;
	Clock::time_point previous_{};
	mutable std::mutex mutex_;
	std::uint64_t frames_ = 0;
	std::uint64_t payload_units_ = 0;
	std::uint64_t invalid_frames_ = 0;
	std::uint32_t width_ = 0;
	std::uint32_t height_ = 0;
	double first_frame_ms_ = -1;
	double max_gap_ms_ = 0;
};

struct ProbeResult {
	std::string type;
	std::string source_id;
	std::string error;
	MetricsSnapshot metrics;
	std::uint64_t callbacks_after_stop = 0;
	bool passed = false;
};

template <typename Capture>
ProbeResult FinishProbe(std::string type, std::string source_id, Capture& capture,
                        FrameMetrics& metrics, std::chrono::seconds duration) {
	std::this_thread::sleep_for(duration);
	capture.Stop();
	const auto stopped_frames = metrics.Snapshot().frames;
	std::this_thread::sleep_for(std::chrono::milliseconds(200));
	ProbeResult result;
	result.type = std::move(type);
	result.source_id = std::move(source_id);
	result.error = capture.LastError();
	result.metrics = metrics.Snapshot();
	result.callbacks_after_stop = result.metrics.frames - stopped_frames;
	result.passed = result.metrics.frames > 0 && result.metrics.invalid_frames == 0 &&
	                result.callbacks_after_stop == 0;
	return result;
}

ProbeResult ProbeAudio(std::string requested_id, std::chrono::seconds duration) {
	media_capture::AudioCaptureConfig config;
	config.device_id = std::move(requested_id);
	FrameMetrics metrics;
	auto capture = media_capture::CreateAudioCapture(config, [&](const auto& frame) {
		metrics.Observe(frame.data != nullptr && frame.sample_rate == config.sample_rate &&
		                    frame.channels == config.channels && frame.frames_per_channel > 0,
		                frame.frames_per_channel);
	});
	if (!capture || !capture->Start()) {
		return {"audio", config.device_id,
		        capture ? capture->LastError() : "failed to create audio capture"};
	}
	auto result = FinishProbe("audio", capture->DeviceId(), *capture, metrics, duration);
	const auto minimum_samples =
	    static_cast<std::uint64_t>(config.sample_rate) * duration.count() * 8U / 10U;
	result.passed = result.passed && result.metrics.payload_units >= minimum_samples;
	return result;
}

ProbeResult ProbeCamera(std::string requested_id, std::chrono::seconds duration,
                        std::uint32_t width, std::uint32_t height,
                        std::uint32_t frames_per_second) {
	media_capture::CameraCaptureConfig config;
	config.device_id = std::move(requested_id);
	config.width = width;
	config.height = height;
	config.frames_per_second = frames_per_second;
	FrameMetrics metrics;
	std::string creation_error;
	auto capture = media_capture::CreateCameraCapture(
	    config,
	    [&](const auto& frame) {
		    const bool dimensions_valid =
		        frame.width > 0 && frame.height > 0 &&
		        frame.width <= std::numeric_limits<std::uint32_t>::max() / 4U;
		    metrics.Observe(frame.data != nullptr && dimensions_valid &&
		                        frame.row_stride_bytes >= frame.width * 4U &&
		                        frame.width == config.width && frame.height == config.height,
		                    1, frame.width, frame.height);
	    },
	    &creation_error);
	if (!capture || !capture->Start()) {
		return {"camera", config.device_id,
		        capture ? capture->LastError() : std::move(creation_error)};
	}
	auto result = FinishProbe("camera", capture->DeviceId(), *capture, metrics, duration);
	const auto expected_frames = static_cast<std::uint64_t>(frames_per_second) * duration.count();
	result.passed = result.passed && result.metrics.frames >= expected_frames * 6U / 10U &&
	                result.metrics.frames <= expected_frames * 12U / 10U + 2U;
	return result;
}

ProbeResult ProbeScreen(std::string type, std::string requested_id, std::chrono::seconds duration) {
	if (requested_id.empty()) {
		const auto wanted_kind = type == "window" ? media_capture::ScreenSourceKind::Window
		                                          : media_capture::ScreenSourceKind::Monitor;
		for (const auto& source : media_capture::EnumerateScreenSources()) {
			if (source.kind == wanted_kind) {
				requested_id = source.id;
				break;
			}
		}
	}
	media_capture::ScreenCaptureConfig config;
	config.source_id = requested_id;
	FrameMetrics metrics;
	auto capture = media_capture::CreateScreenCapture(config, [&](const auto& frame) {
		const bool dimensions_valid = frame.width > 0 && frame.height > 0 &&
		                              frame.width <= std::numeric_limits<std::uint32_t>::max() / 4U;
		metrics.Observe(frame.data != nullptr && dimensions_valid &&
		                    frame.row_stride_bytes >= frame.width * 4U,
		                1, frame.width, frame.height);
	});
	if (!capture || !capture->Start()) {
		return {std::move(type), requested_id,
		        capture ? capture->LastError() : "failed to create screen capture"};
	}
	return FinishProbe(std::move(type), capture->SourceId(), *capture, metrics, duration);
}

void PrintResult(const ProbeResult& result) {
	std::cout << "result=" << (result.passed ? "pass" : "fail") << " type=" << result.type
	          << " source_id=" << std::quoted(result.source_id)
	          << " frames=" << result.metrics.frames
	          << " payload_units=" << result.metrics.payload_units
	          << " invalid_frames=" << result.metrics.invalid_frames << std::fixed
	          << " width=" << result.metrics.width << " height=" << result.metrics.height
	          << std::setprecision(2) << " first_frame_ms=" << result.metrics.first_frame_ms
	          << " max_gap_ms=" << result.metrics.max_gap_ms
	          << " callbacks_after_stop=" << result.callbacks_after_stop
	          << " error=" << std::quoted(result.error) << '\n';
}

} // namespace

int main(int argc, char* argv[]) {
	if (argc < 2 || argc > 7) {
		std::cerr << "Usage: " << argv[0]
		          << " <audio|camera|screen|window> [source-id] [duration-seconds]"
		             " [camera-width camera-height camera-fps]\n";
		return 2;
	}
	const std::string type = argv[1];
	std::string source_id;
	int duration_seconds = 2;
	std::uint32_t camera_width = 1280;
	std::uint32_t camera_height = 720;
	std::uint32_t camera_fps = 30;
	try {
		if (argc == 3 &&
		    std::string_view(argv[2]).find_first_not_of("0123456789") == std::string_view::npos) {
			duration_seconds = std::stoi(argv[2]);
		} else if (argc > 2) {
			source_id = argv[2];
		}
		if (argc >= 4) {
			duration_seconds = std::stoi(argv[3]);
		}
		if (argc > 4) {
			if (type != "camera" || argc != 7) {
				throw std::invalid_argument("camera dimensions require three values");
			}
			camera_width = static_cast<std::uint32_t>(std::stoul(argv[4]));
			camera_height = static_cast<std::uint32_t>(std::stoul(argv[5]));
			camera_fps = static_cast<std::uint32_t>(std::stoul(argv[6]));
		}
	} catch (...) {
		duration_seconds = 0;
	}
	if (source_id == "default") {
		source_id.clear();
	}
	if (duration_seconds <= 0 || duration_seconds > 600 || camera_width == 0 ||
	    camera_height == 0 || camera_fps == 0 || camera_fps > 240) {
		std::cerr << "duration must be 1..600 and camera dimensions/FPS must be positive"
		             " (FPS <= 240)\n";
		return 2;
	}

	ProbeResult result;
	if (type == "audio") {
		result = ProbeAudio(source_id, std::chrono::seconds(duration_seconds));
	} else if (type == "camera") {
		result = ProbeCamera(source_id, std::chrono::seconds(duration_seconds), camera_width,
		                     camera_height, camera_fps);
	} else if (type == "screen" || type == "window") {
		result = ProbeScreen(type, source_id, std::chrono::seconds(duration_seconds));
	} else {
		std::cerr << "unknown capture type: " << type << '\n';
		return 2;
	}
	PrintResult(result);
	return result.passed ? 0 : 1;
}
