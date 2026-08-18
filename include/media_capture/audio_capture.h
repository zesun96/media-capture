/**
 * Copyright (c) 2026 sunze
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "audio_frame.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace media_capture {

struct AudioCaptureConfig {
	std::string device_id;
	std::uint32_t sample_rate = 48000;
	std::uint32_t channels = 1;
};

using AudioFrameCallback = std::function<void(const AudioFrameView& frame)>;

class AudioCapture {
public:
	virtual ~AudioCapture() = default;

	virtual bool Start() = 0;
	virtual void Stop() noexcept = 0;
	virtual bool IsRunning() const noexcept = 0;
	virtual std::string DeviceId() const = 0;
	virtual bool SwitchDevice(std::string_view device_id) = 0;
	virtual std::string LastError() const = 0;
};

std::unique_ptr<AudioCapture> CreateAudioCapture(AudioCaptureConfig config,
                                                 AudioFrameCallback callback);

} // namespace media_capture
