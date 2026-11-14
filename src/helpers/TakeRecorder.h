#ifndef TakeRecorder_h
#define TakeRecorder_h

#include "TakeClock.h"
#include "WavWriter.h"
#include <libremidi/message.hpp>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

struct TakeTiming {
	double tempoMicrosPerQuarter;
	int sigNum;
	int sigDenom;
	// Added to every MIDI time: the instrument sounds a little after it sends.
	double offsetSeconds;
};

/// One take: the instrument's sound to take.wav and the player's MIDI to take.mid, with one time zero.
/// The WAV's sample clock is the take's clock; MIDI host times map onto it through a TakeClock fit.
/// onAudio runs on the capture thread, everything else on the main thread.
class TakeRecorder {
public:

	/// The folder must exist. t0Host is WAV sample 0.
	bool start(const std::string & folder, int rate, double t0Host, const TakeTiming & timing);

	void onAudio(double ptsHost, const float * stereo, size_t frames);

	/// m.timestamp is host seconds.
	void addMIDI(const libremidi::message & m);

	void requestStop(double t1Host);

	/// Call each frame while stopping; true once take.mid is written.
	bool finish(double nowHost);

	/// Between start and a finished stop.
	bool isRecording() const;

	bool isStopping() const;

	double elapsed(double nowHost) const;

	const std::string & folder() const;

	/// Empty when the take is whole and has sound.
	const std::string & error() const;

private:

	double sampleAt(double hostSeconds) const;

	enum class State { IDLE, RECORDING, STOPPING };

	mutable std::mutex _mutex;
	State _state = State::IDLE;
	WavWriter _wav;
	TakeClock _clock{ 48000.0 };
	TakeTiming _timing{};
	std::vector<libremidi::message> _midi;
	std::string _folder;
	std::string _error;
	int _rate = 0;
	float _peak = 0.0f; // Of what is written to take.wav.
	double _t0 = 0.0;
	double _t1 = 0.0;
	bool _aligned = false;
	int64_t _next = 0; // WAV index of the next packet's first sample.
	int64_t _end = 0; // WAV length, set by requestStop.
};

#endif
