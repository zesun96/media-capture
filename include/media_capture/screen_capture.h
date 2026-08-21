/**
 * Copyright (c) 2026 sunze
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "screen_frame.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace media_capture {

struct ScreenCaptureConfig {
	std::string source_id;
	std::uint32_t frames_per_second = 15;
	bool include_cursor = true;
};

using ScreenFrameCallback = std::function<void(const ScreenFrameView& frame)>;

class ScreenCapture {
public:
	virtual ~ScreenCapture() = default;

	virtual bool Start() = 0;
	virtual void Stop() noexcept = 0;
	virtual bool IsRunning() const noexcept = 0;
	virtual std::string SourceId() const = 0;
	virtual std::string LastError() const = 0;
};

// The callback runs on the capture backend thread. Its frame view is valid only for the duration
// of the callback. Stop and object destruction must be performed from another thread.
std::unique_ptr<ScreenCapture> CreateScreenCapture(ScreenCaptureConfig config,
                                                   ScreenFrameCallback callback);

} // namespace media_capture
