/**
 * Copyright (c) 2026 sunze
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "camera_frame.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace media_capture {

struct CameraCaptureConfig {
	std::string device_id;
	std::uint32_t width = 1280;
	std::uint32_t height = 720;
	std::uint32_t frames_per_second = 30;
};

using CameraFrameCallback = std::function<void(const CameraFrameView& frame)>;

class CameraCapture {
public:
	virtual ~CameraCapture() = default;

	virtual bool Start() = 0;
	virtual void Stop() noexcept = 0;
	virtual bool IsRunning() const noexcept = 0;
	virtual std::string DeviceId() const = 0;
	virtual bool SwitchDevice(std::string_view device_id) = 0;
	virtual std::string LastError() const = 0;
};

// Frames are normalized to top-to-bottom BGRA. The callback runs on the backend capture thread,
// and its frame view is valid only for the duration of the callback.
std::unique_ptr<CameraCapture> CreateCameraCapture(CameraCaptureConfig config,
                                                   CameraFrameCallback callback);
// Diagnostic overload. When creation fails, error receives the backend failure without changing
// the existing nullptr-on-failure contract.
std::unique_ptr<CameraCapture>
CreateCameraCapture(CameraCaptureConfig config, CameraFrameCallback callback, std::string* error);

} // namespace media_capture
