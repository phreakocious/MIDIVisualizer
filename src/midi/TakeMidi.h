#ifndef TAKE_MIDI_H
#define TAKE_MIDI_H

#include <libremidi/message.hpp>
#include <ostream>
#include <vector>

/// Write a take's messages as a one-track Standard MIDI File, 960 ticks per quarter note.
/// Each message timestamp is seconds on the take's timeline. Ticks come from the absolute time,
/// so rounding never adds up. End-of-Track lands at lengthSeconds, never before the last event.
void writeTakeMidi(std::ostream& out, std::vector<libremidi::message> messages, double lengthSeconds, double tempoMicrosPerQuarter, int sigNum, int sigDenom);

#endif
