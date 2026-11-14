#ifndef AudioInput_h
#define AudioInput_h

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

struct OpaqueAudioQueue;

/// Captures a stereo pair of an audio device's inputs through a Core Audio input queue.
/// Only built with MIDIVIZ_SUPPORT_AUDIO_INPUT; otherwise there are no devices and open fails.
class AudioInput {
public:

	struct Device { std::string name; int channels; };

	/// Core Audio devices with input channels.
	static std::vector<Device> devices();

	/// Seconds on the clock of CoreMIDI timestamps and of the capture's packet times.
	static double hostNow();

	~AudioInput();

	/// firstChannel is the 1-based first input of the pair. Starts capturing at the device's nominal rate.
	bool open(const std::string & deviceName, int firstChannel);

	/// Once it returns, the sink is not called again until the next open.
	void close();

	bool isOpen() const;

	/// Why the last open failed.
	const std::string & error() const;

	int rate() const;

	/// Peak of the pair since the last call, 0 to 1.
	float takeLevel();

	/// Frames the queue lost since open; the sink got silence in their place, except across a clock jump.
	int64_t droppedFrames() const;

	/// Holes since open that the sample count and host time disagree on, or longer than 2 s: left unfilled.
	int64_t clockJumps() const;

	/// Host seconds when the last buffer arrived, or when the input opened, before the first one.
	double lastBufferHost() const;

	/// Called on the queue's thread with interleaved stereo; ptsHost is its first frame's host time.
	/// Once setSink returns, the previous sink is never called again. Empty to remove.
	/// The sink must not call back into this AudioInput.
	void setSink(std::function<void(double ptsHost, const float * stereo, size_t frames)> sink);

private:

	void deliver(const float * interleaved, size_t frames, double sampleTime, uint64_t hostTime);

	OpaqueAudioQueue * _queue = nullptr;
	std::mutex _mutex; // Guards _sink and _peak.
	std::function<void(double, const float *, size_t)> _sink;
	float _peak = 0.0f;
	std::atomic<int64_t> _dropped{ 0 };
	std::atomic<int64_t> _jumps{ 0 };
	std::atomic<double> _lastBuffer{ 0.0 };
	std::string _error;
	int _rate = 0;
	// Set before the queue starts; the queue's thread only reads them.
	int _channels = 0;
	int _first = 0;
	// The queue's thread only.
	double _nextSample = -1.0;
	double _nextPts = 0.0;
	std::vector<float> _stereo;
};

#endif
