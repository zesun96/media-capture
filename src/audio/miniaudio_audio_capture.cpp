/**
 * Copyright (c) 2026 sunze
 * SPDX-License-Identifier: Apache-2.0
 */

#include "media_capture/audio_capture.h"
#include "media_capture/audio_device.h"

#include "miniaudio.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
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

std::int64_t SteadyTimestampMicros() {
	return std::chrono::duration_cast<std::chrono::microseconds>(
	           std::chrono::steady_clock::now().time_since_epoch())
	    .count();
}

std::string ResultMessage(std::string_view operation, ma_result result) {
	std::string message(operation);
	message += ": ";
	const char* description = ma_result_description(result);
	message += description != nullptr ? description : "unknown miniaudio error";
	message += " (" + std::to_string(result) + ")";
	return message;
}

std::string DescribeNativeFormats(ma_context* context, ma_device_type device_type,
                                  const ma_device_id& id) {
	ma_device_info info{};
	if (ma_context_get_device_info(context, device_type, &id, &info) != MA_SUCCESS) {
		return {};
	}
	std::ostringstream description;
	description << "; native formats:";
	for (ma_uint32 index = 0; index < info.nativeDataFormatCount; ++index) {
		const auto& format = info.nativeDataFormats[index];
		description << " [format=" << static_cast<int>(format.format)
		            << ", channels=" << format.channels << ", rate=" << format.sampleRate << ']';
	}
	return description.str();
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

void AppendDevices(std::vector<AudioDeviceInfo>& result, const ma_device_info* devices,
                   ma_uint32 count, AudioDeviceKind kind) {
	for (ma_uint32 index = 0; index < count; ++index) {
		result.push_back({EncodeDeviceId(devices[index].id), devices[index].name, kind,
		                  devices[index].isDefault == MA_TRUE});
	}
}

class MiniaudioAudioCapture final : public AudioCapture {
public:
	MiniaudioAudioCapture(AudioCaptureConfig config, AudioFrameCallback callback, bool loopback)
	    : config_(std::move(config)), callback_(std::move(callback)), loopback_(loopback) {
		if (!context_.valid()) {
			SetError(ResultMessage("failed to initialize miniaudio context", context_.result()));
		}
	}

	~MiniaudioAudioCapture() override { Stop(); }

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

	std::string LastError() const override {
		std::lock_guard<std::mutex> guard(error_mutex_);
		return last_error_;
	}

private:
	static void DataCallback(ma_device* device, void*, const void* input, ma_uint32 frame_count) {
		auto* self = static_cast<MiniaudioAudioCapture*>(device->pUserData);
		if (self == nullptr || input == nullptr || frame_count == 0 || !self->running_.load()) {
			return;
		}
		const AudioFrameView frame{static_cast<const std::int16_t*>(input),
		                           self->config_.sample_rate, self->config_.channels, frame_count,
		                           SteadyTimestampMicros()};
		try {
			self->callback_(frame);
		} catch (...) {
			self->SetError("audio frame callback threw an exception");
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
		const ma_device_info* devices = loopback_ ? playback_devices : capture_devices;
		const ma_uint32 device_count = loopback_ ? playback_count : capture_count;
		for (ma_uint32 index = 0; index < device_count; ++index) {
			const std::string id = EncodeDeviceId(devices[index].id);
			if ((!requested_id.empty() && requested_id == id) ||
			    (requested_id.empty() && devices[index].isDefault == MA_TRUE)) {
				selected_id = devices[index].id;
				resolved_id = id;
				return true;
			}
		}
		if (requested_id.empty()) {
			resolved_id.clear();
			return true;
		}
		SetError(loopback_ ? "audio output device was not found"
		                   : "audio input device was not found");
		return false;
	}

	bool StartLocked(const std::string& requested_id) {
		if (running_.load()) {
			return true;
		}
		if (!context_.valid() || !callback_ || config_.sample_rate == 0 || config_.channels == 0) {
			SetError("audio capture configuration is invalid");
			return false;
		}

		ma_device_id selected_id{};
		std::string resolved_id;
		if (!FindDevice(requested_id, selected_id, resolved_id)) {
			return false;
		}
		ma_device_config device_config =
		    ma_device_config_init(loopback_ ? ma_device_type_loopback : ma_device_type_capture);
		device_config.capture.format = ma_format_s16;
		device_config.capture.channels = config_.channels;
		device_config.capture.pDeviceID = requested_id.empty() ? nullptr : &selected_id;
		device_config.sampleRate = config_.sample_rate;
		device_config.dataCallback = DataCallback;
		device_config.pUserData = this;

		ma_result result = ma_device_init(context_.get(), &device_config, &device_);
		if (result != MA_SUCCESS) {
			SetError(
			    ResultMessage(loopback_ ? "failed to initialize system audio capture"
			                            : "failed to initialize audio capture device",
			                  result) +
			    DescribeNativeFormats(context_.get(),
			                          loopback_ ? ma_device_type_playback : ma_device_type_capture,
			                          selected_id));
			return false;
		}
		device_initialized_ = true;
		running_.store(true);
		result = ma_device_start(&device_);
		if (result != MA_SUCCESS) {
			running_.store(false);
			ma_device_uninit(&device_);
			device_initialized_ = false;
			SetError(ResultMessage("failed to start audio capture device", result));
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
	}

	void SetError(std::string message) const {
		std::lock_guard<std::mutex> guard(error_mutex_);
		last_error_ = std::move(message);
	}

	AudioCaptureConfig config_;
	AudioFrameCallback callback_;
	bool loopback_ = false;
	Context context_;
	ma_device device_{};
	bool device_initialized_ = false;
	std::atomic_bool running_{false};
	mutable std::mutex lifecycle_mutex_;
	mutable std::mutex state_mutex_;
	std::string active_device_id_;
	mutable std::mutex error_mutex_;
	mutable std::string last_error_;
};

} // namespace

std::vector<AudioDeviceInfo> EnumerateAudioDevices() {
	Context context;
	if (!context.valid()) {
		return {};
	}
	ma_device_info* playback_devices = nullptr;
	ma_uint32 playback_count = 0;
	ma_device_info* capture_devices = nullptr;
	ma_uint32 capture_count = 0;
	if (ma_context_get_devices(context.get(), &playback_devices, &playback_count, &capture_devices,
	                           &capture_count) != MA_SUCCESS) {
		return {};
	}
	std::vector<AudioDeviceInfo> result;
	result.reserve(static_cast<std::size_t>(playback_count) + capture_count);
	AppendDevices(result, capture_devices, capture_count, AudioDeviceKind::Input);
	AppendDevices(result, playback_devices, playback_count, AudioDeviceKind::Output);
	return result;
}

std::unique_ptr<AudioCapture> CreateAudioCapture(AudioCaptureConfig config,
                                                 AudioFrameCallback callback) {
	if (!callback || config.sample_rate == 0 || config.channels == 0) {
		return nullptr;
	}
	return std::make_unique<MiniaudioAudioCapture>(std::move(config), std::move(callback), false);
}

std::unique_ptr<AudioCapture> CreateSystemAudioCapture(AudioCaptureConfig config,
                                                       AudioFrameCallback callback) {
	if (!callback || config.sample_rate == 0 || config.channels == 0) {
		return nullptr;
	}
	return std::make_unique<MiniaudioAudioCapture>(std::move(config), std::move(callback), true);
}

} // namespace media_capture
