/**
 * Copyright (c) 2026 sunze
 * SPDX-License-Identifier: Apache-2.0
 */

#include "media_capture/audio_playback.h"

#include "miniaudio.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace media_capture {
namespace {

constexpr std::string_view kDeviceIdPrefix = "miniaudio:";

std::string EncodeDeviceId(const ma_device_id& id) {
	const auto* bytes = reinterpret_cast<const std::uint8_t*>(&id);
	std::size_t byte_count = sizeof(id);
	while (byte_count > 0 && bytes[byte_count - 1] == 0) {
		--byte_count;
	}
	std::ostringstream result;
	result << kDeviceIdPrefix << std::hex << std::setfill('0');
	for (std::size_t index = 0; index < byte_count; ++index) {
		result << std::setw(2) << static_cast<unsigned int>(bytes[index]);
	}
	return result.str();
}

std::string ResultMessage(std::string_view operation, ma_result result) {
	std::string message(operation);
	message += ": ";
	const char* description = ma_result_description(result);
	message += description != nullptr ? description : "unknown miniaudio error";
	message += " (" + std::to_string(result) + ")";
	return message;
}

class Context {
public:
	Context() : result_(ma_context_init(nullptr, 0, nullptr, &value_)) {}
	~Context() {
		if (result_ == MA_SUCCESS) {
			ma_context_uninit(&value_);
		}
	}

	Context(const Context&) = delete;
	Context& operator=(const Context&) = delete;

	bool valid() const noexcept { return result_ == MA_SUCCESS; }
	ma_result result() const noexcept { return result_; }
	ma_context* get() noexcept { return &value_; }

private:
	ma_context value_{};
	ma_result result_ = MA_ERROR;
};

class MiniaudioAudioPlayback final : public AudioPlayback {
public:
	explicit MiniaudioAudioPlayback(AudioPlaybackConfig config) : config_(std::move(config)) {
		if (!context_.valid()) {
			SetError(ResultMessage("failed to initialize miniaudio context", context_.result()));
		}
	}

	~MiniaudioAudioPlayback() override { Stop(); }

	bool Start() override {
		std::lock_guard<std::mutex> guard(lifecycle_mutex_);
		return StartLocked(config_.device_id);
	}

	void Stop() noexcept override {
		std::lock_guard<std::mutex> guard(lifecycle_mutex_);
		StopLocked();
	}

	bool IsRunning() const noexcept override { return running_.load(); }

	std::string DeviceId() const override {
		std::lock_guard<std::mutex> guard(state_mutex_);
		return active_device_id_;
	}

	bool SwitchDevice(std::string_view device_id) override {
		if (device_id.empty()) {
			SetError("device ID must not be empty");
			return false;
		}
		std::lock_guard<std::mutex> guard(lifecycle_mutex_);
		if (device_id == config_.device_id && running_.load()) {
			return true;
		}
		const std::string previous_id = config_.device_id;
		const bool was_running = running_.load();
		StopLocked();
		if (!was_running) {
			config_.device_id = device_id;
			std::lock_guard<std::mutex> state_guard(state_mutex_);
			active_device_id_ = device_id;
			return true;
		}
		if (StartLocked(std::string(device_id))) {
			return true;
		}
		const std::string switch_error = LastError();
		StartLocked(previous_id);
		SetError(switch_error);
		return false;
	}

	bool QueueFrame(const AudioFrameView& frame) override {
		if (!running_.load()) {
			SetError("audio playback is not running");
			return false;
		}
		if (frame.data == nullptr || frame.frames_per_channel == 0 ||
		    frame.sample_rate != config_.sample_rate || frame.channels != config_.channels) {
			SetError("audio frame format does not match playback configuration");
			return false;
		}

		std::uint32_t source_offset = 0;
		std::uint32_t frames_to_write = frame.frames_per_channel;
		std::lock_guard<std::mutex> guard(buffer_mutex_);
		if (frames_to_write > capacity_frames_) {
			source_offset = frames_to_write - capacity_frames_;
			dropped_frames_.fetch_add(source_offset);
			frames_to_write = capacity_frames_;
		}
		const std::uint32_t free_frames = capacity_frames_ - buffered_frames_;
		if (frames_to_write > free_frames) {
			const std::uint32_t overflow = frames_to_write - free_frames;
			read_frame_ = (read_frame_ + overflow) % capacity_frames_;
			buffered_frames_ -= overflow;
			dropped_frames_.fetch_add(overflow);
		}
		WriteFrames(frame.data + static_cast<std::size_t>(source_offset) * config_.channels,
		            frames_to_write);
		queued_frames_.fetch_add(frames_to_write);
		SetError({});
		return true;
	}

	bool SetVolume(float volume) override {
		if (!std::isfinite(volume) || volume < 0.0F || volume > 1.0F) {
			SetError("audio playback volume must be between 0 and 1");
			return false;
		}
		volume_.store(volume);
		SetError({});
		return true;
	}

	float Volume() const noexcept override { return volume_.load(); }

	void SetMuted(bool muted) noexcept override { muted_.store(muted); }

	bool IsMuted() const noexcept override { return muted_.load(); }

	AudioPlaybackStats Stats() const noexcept override {
		AudioPlaybackStats result;
		result.queued_frames = queued_frames_.load();
		result.played_frames = played_frames_.load();
		result.dropped_frames = dropped_frames_.load();
		result.underrun_frames = underrun_frames_.load();
		{
			std::lock_guard<std::mutex> guard(buffer_mutex_);
			result.buffered_frames = buffered_frames_;
		}
		result.buffered_duration_ms =
		    config_.sample_rate == 0
		        ? 0
		        : static_cast<std::uint32_t>(static_cast<std::uint64_t>(result.buffered_frames) *
		                                     1000 / config_.sample_rate);
		result.device_latency_ms = device_latency_ms_.load();
		result.estimated_delay_ms = result.buffered_duration_ms + result.device_latency_ms;
		return result;
	}

	std::string LastError() const override {
		std::lock_guard<std::mutex> guard(error_mutex_);
		return last_error_;
	}

private:
	static void DataCallback(ma_device* device, void* output, const void*, ma_uint32 frame_count) {
		auto* self = static_cast<MiniaudioAudioPlayback*>(device->pUserData);
		if (self == nullptr || output == nullptr || frame_count == 0) {
			return;
		}
		self->Render(static_cast<std::int16_t*>(output), frame_count);
	}

	void Render(std::int16_t* output, std::uint32_t frame_count) noexcept {
		const std::size_t sample_count = static_cast<std::size_t>(frame_count) * config_.channels;
		std::memset(output, 0, sample_count * sizeof(std::int16_t));
		if (!running_.load()) {
			return;
		}

		std::uint32_t frames_read = 0;
		{
			std::lock_guard<std::mutex> guard(buffer_mutex_);
			frames_read = std::min(frame_count, buffered_frames_);
			ReadFrames(output, frames_read);
		}
		played_frames_.fetch_add(frames_read);
		underrun_frames_.fetch_add(frame_count - frames_read);

		const float volume = muted_.load() ? 0.0F : volume_.load();
		if (volume == 1.0F) {
			return;
		}
		for (std::size_t index = 0; index < sample_count; ++index) {
			output[index] = static_cast<std::int16_t>(std::clamp(
			    static_cast<long>(std::lround(static_cast<float>(output[index]) * volume)),
			    static_cast<long>(std::numeric_limits<std::int16_t>::min()),
			    static_cast<long>(std::numeric_limits<std::int16_t>::max())));
		}
	}

	bool FindDevice(std::string_view requested_id, ma_device_id& selected_id,
	                std::string& resolved_id) {
		ma_device_info* playback_devices = nullptr;
		ma_uint32 playback_count = 0;
		ma_device_info* capture_devices = nullptr;
		ma_uint32 capture_count = 0;
		const ma_result result = ma_context_get_devices(
		    context_.get(), &playback_devices, &playback_count, &capture_devices, &capture_count);
		if (result != MA_SUCCESS) {
			SetError(ResultMessage("failed to enumerate audio devices", result));
			return false;
		}
		for (ma_uint32 index = 0; index < playback_count; ++index) {
			const std::string id = EncodeDeviceId(playback_devices[index].id);
			if ((!requested_id.empty() && requested_id == id) ||
			    (requested_id.empty() && playback_devices[index].isDefault == MA_TRUE)) {
				selected_id = playback_devices[index].id;
				resolved_id = id;
				return true;
			}
		}
		if (requested_id.empty()) {
			resolved_id.clear();
			return true;
		}
		SetError("audio output device was not found");
		return false;
	}

	bool StartLocked(const std::string& requested_id) {
		if (running_.load()) {
			return true;
		}
		const std::uint64_t capacity =
		    static_cast<std::uint64_t>(config_.sample_rate) * config_.buffer_duration_ms / 1000;
		if (!context_.valid() || config_.sample_rate == 0 || config_.channels == 0 ||
		    config_.channels > 2 || capacity == 0 || config_.buffer_duration_ms > 5000 ||
		    capacity > std::numeric_limits<std::uint32_t>::max()) {
			SetError("audio playback configuration is invalid");
			return false;
		}

		ma_device_id selected_id{};
		std::string resolved_id;
		if (!FindDevice(requested_id, selected_id, resolved_id)) {
			return false;
		}
		capacity_frames_ = static_cast<std::uint32_t>(capacity);
		try {
			std::lock_guard<std::mutex> guard(buffer_mutex_);
			buffer_.assign(static_cast<std::size_t>(capacity_frames_) * config_.channels, 0);
			read_frame_ = 0;
			write_frame_ = 0;
			buffered_frames_ = 0;
		} catch (...) {
			SetError("failed to allocate audio playback buffer");
			return false;
		}

		ma_device_config device_config = ma_device_config_init(ma_device_type_playback);
		device_config.playback.format = ma_format_s16;
		device_config.playback.channels = config_.channels;
		device_config.playback.pDeviceID = requested_id.empty() ? nullptr : &selected_id;
		device_config.sampleRate = config_.sample_rate;
		device_config.periodSizeInMilliseconds = 10;
		device_config.periods = 2;
		device_config.dataCallback = DataCallback;
		device_config.pUserData = this;

		ma_result result = ma_device_init(context_.get(), &device_config, &device_);
		if (result != MA_SUCCESS) {
			SetError(ResultMessage("failed to initialize audio playback device", result));
			return false;
		}
		device_initialized_ = true;
		const std::uint64_t device_latency_frames =
		    static_cast<std::uint64_t>(device_.playback.internalPeriodSizeInFrames) *
		    device_.playback.internalPeriods;
		device_latency_ms_.store(
		    device_.playback.internalSampleRate == 0
		        ? 0
		        : static_cast<std::uint32_t>(device_latency_frames * 1000 /
		                                     device_.playback.internalSampleRate));
		running_.store(true);
		result = ma_device_start(&device_);
		if (result != MA_SUCCESS) {
			running_.store(false);
			ma_device_uninit(&device_);
			device_initialized_ = false;
			SetError(ResultMessage("failed to start audio playback device", result));
			return false;
		}
		config_.device_id = requested_id;
		{
			std::lock_guard<std::mutex> guard(state_mutex_);
			active_device_id_ = std::move(resolved_id);
		}
		SetError({});
		return true;
	}

	void StopLocked() noexcept {
		running_.store(false);
		if (device_initialized_) {
			ma_device_stop(&device_);
			ma_device_uninit(&device_);
			device_initialized_ = false;
		}
		std::lock_guard<std::mutex> guard(buffer_mutex_);
		buffered_frames_ = 0;
		read_frame_ = 0;
		write_frame_ = 0;
	}

	void WriteFrames(const std::int16_t* source, std::uint32_t frame_count) noexcept {
		const std::uint32_t first_frames = std::min(frame_count, capacity_frames_ - write_frame_);
		std::memcpy(
		    buffer_.data() + static_cast<std::size_t>(write_frame_) * config_.channels, source,
		    static_cast<std::size_t>(first_frames) * config_.channels * sizeof(std::int16_t));
		const std::uint32_t remaining_frames = frame_count - first_frames;
		if (remaining_frames > 0) {
			std::memcpy(buffer_.data(),
			            source + static_cast<std::size_t>(first_frames) * config_.channels,
			            static_cast<std::size_t>(remaining_frames) * config_.channels *
			                sizeof(std::int16_t));
		}
		write_frame_ = (write_frame_ + frame_count) % capacity_frames_;
		buffered_frames_ += frame_count;
	}

	void ReadFrames(std::int16_t* output, std::uint32_t frame_count) noexcept {
		const std::uint32_t first_frames = std::min(frame_count, capacity_frames_ - read_frame_);
		std::memcpy(
		    output, buffer_.data() + static_cast<std::size_t>(read_frame_) * config_.channels,
		    static_cast<std::size_t>(first_frames) * config_.channels * sizeof(std::int16_t));
		const std::uint32_t remaining_frames = frame_count - first_frames;
		if (remaining_frames > 0) {
			std::memcpy(output + static_cast<std::size_t>(first_frames) * config_.channels,
			            buffer_.data(),
			            static_cast<std::size_t>(remaining_frames) * config_.channels *
			                sizeof(std::int16_t));
		}
		read_frame_ = (read_frame_ + frame_count) % capacity_frames_;
		buffered_frames_ -= frame_count;
	}

	void SetError(std::string message) const {
		std::lock_guard<std::mutex> guard(error_mutex_);
		last_error_ = std::move(message);
	}

	AudioPlaybackConfig config_;
	Context context_;
	ma_device device_{};
	bool device_initialized_ = false;
	std::atomic_bool running_{false};
	std::atomic<float> volume_{1.0F};
	std::atomic_bool muted_{false};
	mutable std::mutex lifecycle_mutex_;
	mutable std::mutex state_mutex_;
	std::string active_device_id_;
	mutable std::mutex buffer_mutex_;
	std::vector<std::int16_t> buffer_;
	std::uint32_t capacity_frames_ = 0;
	std::uint32_t read_frame_ = 0;
	std::uint32_t write_frame_ = 0;
	std::uint32_t buffered_frames_ = 0;
	std::atomic<std::uint64_t> queued_frames_{0};
	std::atomic<std::uint64_t> played_frames_{0};
	std::atomic<std::uint64_t> dropped_frames_{0};
	std::atomic<std::uint64_t> underrun_frames_{0};
	std::atomic<std::uint32_t> device_latency_ms_{0};
	mutable std::mutex error_mutex_;
	mutable std::string last_error_;
};

} // namespace

std::unique_ptr<AudioPlayback> CreateAudioPlayback(AudioPlaybackConfig config) {
	if (config.sample_rate == 0 || config.channels == 0 || config.channels > 2 ||
	    config.buffer_duration_ms == 0 || config.buffer_duration_ms > 5000) {
		return nullptr;
	}
	return std::make_unique<MiniaudioAudioPlayback>(std::move(config));
}

} // namespace media_capture
