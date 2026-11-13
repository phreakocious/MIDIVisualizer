#include "TakeRecorder.h"

#include "../midi/TakeMidi.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <utility>

bool TakeRecorder::start(const std::string & folder, int rate, double t0Host, const TakeTiming & timing) {
	std::lock_guard<std::mutex> lock(_mutex);
	if(_state != State::IDLE) {
		return false;
	}
	_folder = folder;
	_error.clear();
	if(!_wav.open(folder + "/take.wav", rate, 2)) {
		_error = "Could not create take.wav in " + folder + ".";
		return false;
	}
	_rate = rate;
	_t0 = _t1 = t0Host;
	_timing = timing;
	_clock = TakeClock(rate);
	_midi.clear();
	_aligned = false;
	_next = _end = 0;
	_state = State::RECORDING;
	return true;
}

void TakeRecorder::onAudio(double ptsHost, const float * stereo, size_t frames) {
	std::lock_guard<std::mutex> lock(_mutex);
	if(_state == State::IDLE) {
		return;
	}
	if(!_aligned) {
		// The first packet that reaches t0 sets sample 0; after it, samples go back to back, never re-aligned.
		const int64_t cut = std::llround((_t0 - ptsHost) * _rate);
		if(cut >= int64_t(frames)) {
			return;
		}
		_next = -cut;
		_aligned = true;
	}
	const int64_t first = _next;
	_clock.add(ptsHost, double(first));
	_next += int64_t(frames);

	const int64_t limit = _state == State::STOPPING ? _end : _next;
	// Only the first packet can start past what is written: it starts after t0.
	const int64_t gap = std::min(first, limit) - _wav.frames();
	if(gap > 0) {
		const std::vector<float> silence(size_t(gap) * 2, 0.0f);
		_wav.write(silence.data(), size_t(gap));
	}
	const int64_t from = _wav.frames();
	const int64_t to = std::min(_next, limit);
	if(to > from) {
		_wav.write(stereo + (from - first) * 2, size_t(to - from));
	}
}

void TakeRecorder::addMIDI(const libremidi::message & m) {
	// Channel messages only: writeTakeMidi needs bytes, and system bytes (a reset is 0xFF) would break the track.
	if(m.bytes.empty() || m.bytes[0] < 0x80 || m.bytes[0] >= 0xF0) {
		return;
	}
	std::lock_guard<std::mutex> lock(_mutex);
	if(_state == State::IDLE || m.timestamp < _t0 || (_state == State::STOPPING && m.timestamp >= _t1)) {
		return;
	}
	_midi.push_back(m);
}

void TakeRecorder::requestStop(double t1Host) {
	std::lock_guard<std::mutex> lock(_mutex);
	if(_state != State::RECORDING) {
		return;
	}
	_t1 = t1Host;
	// Written samples cannot be taken back, so the take never ends before them.
	_end = std::max<int64_t>(std::llround(sampleAt(t1Host)), _wav.frames());
	_state = State::STOPPING;
}

bool TakeRecorder::finish(double nowHost) {
	std::vector<libremidi::message> midi;
	{
		std::lock_guard<std::mutex> lock(_mutex);
		if(_state != State::STOPPING) {
			return false;
		}
		const bool whole = _wav.frames() >= _end;
		if(!whole && nowHost < _t1 + 1.0) {
			return false;
		}
		if(!whole) {
			_error = "The sound stopped before the end of the take.";
		}
		// From here onAudio leaves the WAV and the clock alone, so the files are written outside the lock.
		_state = State::IDLE;
		midi.swap(_midi);
	}
	if(!_wav.close() && _error.empty()) {
		_error = "Could not write take.wav.";
	}

	std::stable_sort(midi.begin(), midi.end(), [](const libremidi::message & a, const libremidi::message & b) {
		return a.timestamp < b.timestamp;
	});
	const double length = double(_end) / _rate;
	bool down[16][128] = {};
	bool pedal[16] = {};
	for(libremidi::message & m : midi) {
		// Clamped to the take, so take.mid never outruns take.wav; equal times keep their order through the stable sorts.
		m.timestamp = std::min(std::max(sampleAt(m.timestamp) / _rate + _timing.offsetSeconds, 0.0), length);
		if(m.size() < 3) {
			continue;
		}
		const int type = m.bytes[0] & 0xF0;
		const int channel = m.bytes[0] & 0x0F;
		if(type == 0x80 || type == 0x90) {
			down[channel][m.bytes[1] & 0x7F] = type == 0x90 && m.bytes[2] > 0;
		} else if(type == 0xB0 && m.bytes[1] == 64) {
			pedal[channel] = m.bytes[2] >= 64;
		}
	}
	// Release what is still held when the take ends.
	for(int channel = 0; channel < 16; ++channel) {
		for(int note = 0; note < 128; ++note) {
			if(down[channel][note]) {
				midi.push_back(libremidi::message::note_off(uint8_t(channel + 1), uint8_t(note), 0));
				midi.back().timestamp = length;
			}
		}
		if(pedal[channel]) {
			midi.push_back(libremidi::message::control_change(uint8_t(channel + 1), 64, 0));
			midi.back().timestamp = length;
		}
	}

	std::ofstream out(_folder + "/take.mid", std::ios::binary | std::ios::trunc);
	writeTakeMidi(out, std::move(midi), length, _timing.tempoMicrosPerQuarter, _timing.sigNum, _timing.sigDenom);
	out.close();
	if(!out && _error.empty()) {
		_error = "Could not write take.mid.";
	}
	return true;
}

bool TakeRecorder::isRecording() const {
	std::lock_guard<std::mutex> lock(_mutex);
	return _state != State::IDLE;
}

bool TakeRecorder::isStopping() const {
	std::lock_guard<std::mutex> lock(_mutex);
	return _state == State::STOPPING;
}

double TakeRecorder::elapsed(double nowHost) const {
	std::lock_guard<std::mutex> lock(_mutex);
	return (_state == State::RECORDING ? nowHost : _t1) - _t0;
}

const std::string & TakeRecorder::folder() const {
	std::lock_guard<std::mutex> lock(_mutex);
	return _folder;
}

const std::string & TakeRecorder::error() const {
	std::lock_guard<std::mutex> lock(_mutex);
	return _error;
}

double TakeRecorder::sampleAt(double hostSeconds) const {
	// Before any audio, the nominal rate from t0 is all there is.
	return _clock.count() > 0 ? _clock.sampleAt(hostSeconds) : (hostSeconds - _t0) * _rate;
}
