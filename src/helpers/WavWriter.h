#ifndef WavWriter_h
#define WavWriter_h

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>

// 24-bit PCM, streamed to disk: a take can be long, so nothing stays in memory.
class WavWriter {
public:
	bool open(const std::string & path, int rate, int channels);
	void write(const float * interleaved, size_t frames);
	int64_t frames() const { return _frames; }
	// Rewrites the RIFF and data sizes.
	bool close();

	// Accepts only 24-bit PCM.
	static bool readInfo(const std::string & path, int & rate, int & channels, int64_t & frames, int64_t & dataOffset);
	// Reads the next frames from a stream positioned at the data (or later).
	static bool readFrames(std::ifstream & in, int channels, float * interleaved, size_t frames);

private:
	std::ofstream _out;
	int _channels = 0;
	int64_t _frames = 0;
};

#endif
