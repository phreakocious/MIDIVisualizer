#include "TakeMidi.h"

#include <libremidi/writer.hpp>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>

void writeTakeMidi(std::ostream& out, std::vector<libremidi::message> messages, double lengthSeconds, double tempoMicrosPerQuarter, int sigNum, int sigDenom){
	const double ticksPerQuarter = 960.0;
	const double ticksPerSecond = ticksPerQuarter * 1000000.0 / tempoMicrosPerQuarter;
	auto tickAt = [&](double t){ return (long long)std::llround((std::max)(t, 0.0) * ticksPerSecond); };

	std::stable_sort(messages.begin(), messages.end(), [](const libremidi::message& a, const libremidi::message& b){
		return a.timestamp < b.timestamp;
	});

	libremidi::writer writer;
	writer.ticksPerQuarterNote = int(ticksPerQuarter);
	writer.tracks.resize(1);
	// Same metas as MIDISceneLive::save; the writer takes the tick of each event as a delta.
	writer.add_event(0, 0, libremidi::meta_events::tempo(int(tempoMicrosPerQuarter)));
	writer.add_event(0, 0, libremidi::meta_events::time_signature(sigNum, sigDenom));
	writer.add_event(0, 0, libremidi::meta_events::key_signature(1, false));

	long long previous = 0;
	for(const libremidi::message& message : messages){
		const long long tick = tickAt(message.timestamp);
		writer.add_event(int(tick - previous), 0, message);
		previous = tick;
	}

	std::stringstream buffer;
	writer.write(buffer);
	std::string bytes = buffer.str();

	// The writer drops explicit End-of-Track events and appends its own at delta 0 (00 FF 2F 00).
	// Swap that for one at the take's length, and fix the track length (bytes 18..21).
	bytes.resize(bytes.size() - 4);
	uint32_t delta = uint32_t((std::max)(tickAt(lengthSeconds), previous) - previous);
	std::string vlv(1, char(delta & 0x7F));
	while((delta >>= 7) != 0){
		vlv.insert(vlv.begin(), char(0x80 | (delta & 0x7F)));
	}
	bytes += vlv;
	bytes += "\xFF\x2F";
	bytes += '\0';
	const uint32_t trackSize = uint32_t(bytes.size() - 22);
	for(int i = 0; i < 4; ++i){
		bytes[18 + i] = char((trackSize >> (24 - 8 * i)) & 0xFF);
	}
	out.write(bytes.data(), std::streamsize(bytes.size()));
}
