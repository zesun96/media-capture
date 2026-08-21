#include "media_capture/audio_playback.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <numbers>
#include <string>
#include <thread>
#include <utility>
#include <vector>

int main(int argc, char* argv[]) {
	media_capture::AudioPlaybackConfig config;
	if (argc > 1) {
		config.device_id = argv[1];
	}
	auto playback = media_capture::CreateAudioPlayback(std::move(config));
	if (!playback || !playback->Start()) {
		std::cerr << "Failed to start audio playback: "
		          << (playback ? playback->LastError() : "invalid configuration") << '\n';
		return 1;
	}

	constexpr std::uint32_t sample_rate = 48000;
	constexpr std::uint32_t channels = 2;
	constexpr std::uint32_t frames_per_block = sample_rate / 100;
	constexpr double frequency = 440.0;
	std::vector<std::int16_t> samples(frames_per_block * channels);
	std::uint64_t generated_frames = 0;
	const auto started_at = std::chrono::steady_clock::now();
	for (std::uint32_t block = 0; block < 300; ++block) {
		if (block == 150 && argc > 2 && !playback->SwitchDevice(argv[2])) {
			std::cerr << "Failed to switch audio playback device: " << playback->LastError()
			          << '\n';
			return 1;
		}
		for (std::uint32_t frame = 0; frame < frames_per_block; ++frame) {
			const double phase = 2.0 * std::numbers::pi * frequency *
			                     static_cast<double>(generated_frames + frame) / sample_rate;
			const auto sample = static_cast<std::int16_t>(std::sin(phase) * 4000.0);
			samples[frame * channels] = sample;
			samples[frame * channels + 1] = sample;
		}
		if (!playback->QueueFrame({samples.data(), sample_rate, channels, frames_per_block, 0})) {
			std::cerr << "Failed to queue audio: " << playback->LastError() << '\n';
			return 1;
		}
		generated_frames += frames_per_block;
		std::this_thread::sleep_until(started_at + std::chrono::milliseconds((block + 1) * 10));
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	const auto stats = playback->Stats();
	std::cout << "Played device " << playback->DeviceId() << ": queued=" << stats.queued_frames
	          << ", played=" << stats.played_frames << ", dropped=" << stats.dropped_frames
	          << ", underrun=" << stats.underrun_frames
	          << ", buffered_ms=" << stats.buffered_duration_ms
	          << ", device_latency_ms=" << stats.device_latency_ms
	          << ", estimated_delay_ms=" << stats.estimated_delay_ms << '\n';
	playback->Stop();
	return stats.played_frames == 0 || stats.dropped_frames != 0 ? 1 : 0;
}
