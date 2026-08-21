/**
 * Copyright (c) 2026 sunze
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "audio_frame.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace media_capture {

struct AudioPlaybackConfig {
	std::string device_id;
	std::uint32_t sample_rate = 48000;
	std::uint32_t channels = 2;
	std::uint32_t buffer_duration_ms = 200;
};

struct AudioPlaybackStats {
	std::uint64_t queued_frames = 0;
	std::uint64_t played_frames = 0;
	std::uint64_t dropped_frames = 0;
	std::uint64_t underrun_frames = 0;
	std::uint32_t buffered_frames = 0;
	std::uint32_t buffered_duration_ms = 0;
	std::uint32_t device_latency_ms = 0;
	std::uint32_t estimated_delay_ms = 0;
};

class AudioPlayback {
public:
	virtual ~AudioPlayback() = default;

	virtual bool Start() = 0;
	virtual void Stop() noexcept = 0;
	virtual bool IsRunning() const noexcept = 0;
	virtual std::string DeviceId() const = 0;
	virtual bool SwitchDevice(std::string_view device_id) = 0;
	virtual bool QueueFrame(const AudioFrameView& frame) = 0;
	virtual bool SetVolume(float volume) = 0;
	virtual float Volume() const noexcept = 0;
	virtual void SetMuted(bool muted) noexcept = 0;
	virtual bool IsMuted() const noexcept = 0;
	virtual AudioPlaybackStats Stats() const noexcept = 0;
	virtual std::string LastError() const = 0;
};

std::unique_ptr<AudioPlayback> CreateAudioPlayback(AudioPlaybackConfig config = {});

} // namespace media_capture
