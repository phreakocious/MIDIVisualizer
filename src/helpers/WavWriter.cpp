#include "WavWriter.h"

#include <algorithm>
#include <cmath>
#include <vector>

static const float kScale = 8388607.0f;

static void put32(unsigned char * p, uint32_t v) {
	for(int i = 0; i < 4; ++i) { p[i] = (unsigned char)(v >> (8 * i)); }
}

static void put16(unsigned char * p, uint16_t v) {
	p[0] = (unsigned char)v;
	p[1] = (unsigned char)(v >> 8);
}

static uint32_t get32(const unsigned char * p) {
	return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

static uint16_t get16(const unsigned char * p) {
	return uint16_t(p[0] | p[1] << 8);
}

bool WavWriter::open(const std::string & path, int rate, int channels) {
	if(rate <= 0 || channels <= 0) {
		return false;
	}
	_out.open(path, std::ios::binary | std::ios::trunc);
	if(!_out) {
		return false;
	}
	_channels = channels;
	_frames = 0;
	// Sizes stay 0 until close() knows them.
	unsigned char h[44] = { 'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 24, 0, 'd', 'a', 't', 'a', 0, 0, 0, 0 };
	put16(h + 22, (uint16_t)channels);
	put32(h + 24, (uint32_t)rate);
	put32(h + 28, (uint32_t)rate * channels * 3);
	put16(h + 32, (uint16_t)(channels * 3));
	_out.write(reinterpret_cast<const char *>(h), 44);
	return bool(_out);
}

void WavWriter::write(const float * interleaved, size_t frames) {
	const size_t n = frames * _channels;
	std::vector<unsigned char> bytes(n * 3);
	for(size_t i = 0; i < n; ++i) {
		const float s = std::max(-1.0f, std::min(1.0f, interleaved[i]));
		const int32_t v = (int32_t)std::lround(s * kScale);
		bytes[3 * i] = (unsigned char)(v & 0xFF);
		bytes[3 * i + 1] = (unsigned char)((v >> 8) & 0xFF);
		bytes[3 * i + 2] = (unsigned char)((v >> 16) & 0xFF);
	}
	_out.write(reinterpret_cast<const char *>(bytes.data()), (std::streamsize)bytes.size());
	_frames += (int64_t)frames;
}

bool WavWriter::close() {
	if(!_out.is_open()) {
		return false;
	}
	// shortcut: 32-bit RIFF sizes cap a file at 4 GiB (about 7 h of 48 kHz stereo); move to RF64 if takes get longer.
	const uint32_t data = (uint32_t)(_frames * _channels * 3);
	unsigned char b[4];
	put32(b, 36 + data);
	_out.seekp(4);
	_out.write(reinterpret_cast<const char *>(b), 4);
	put32(b, data);
	_out.seekp(40);
	_out.write(reinterpret_cast<const char *>(b), 4);
	_out.close();
	return !_out.fail();
}

bool WavWriter::readInfo(const std::string & path, int & rate, int & channels, int64_t & frames, int64_t & dataOffset) {
	std::ifstream in(path, std::ios::binary);
	unsigned char h[12];
	if(!in.read(reinterpret_cast<char *>(h), 12) || std::string((char *)h, 4) != "RIFF" || std::string((char *)h + 8, 4) != "WAVE") {
		return false;
	}
	bool haveFmt = false;
	unsigned char c[8];
	// Walk the chunks: other writers put LIST and the like before data.
	while(in.read(reinterpret_cast<char *>(c), 8)) {
		const std::string id((char *)c, 4);
		const int64_t size = get32(c + 4);
		if(id == "fmt ") {
			unsigned char f[16];
			if(size < 16 || !in.read(reinterpret_cast<char *>(f), 16)) {
				return false;
			}
			if(get16(f) != 1 || get16(f + 14) != 24) {
				return false;
			}
			channels = get16(f + 2);
			rate = (int)get32(f + 4);
			haveFmt = channels > 0;
			in.seekg(size - 16 + (size & 1), std::ios::cur);
		} else if(id == "data") {
			if(!haveFmt) {
				return false;
			}
			dataOffset = (int64_t)in.tellg();
			frames = size / (3 * channels);
			return true;
		} else {
			in.seekg(size + (size & 1), std::ios::cur);
		}
	}
	return false;
}

bool WavWriter::readFrames(std::ifstream & in, int channels, float * interleaved, size_t frames) {
	const size_t n = frames * channels;
	std::vector<unsigned char> bytes(n * 3);
	if(!in.read(reinterpret_cast<char *>(bytes.data()), (std::streamsize)bytes.size())) {
		return false;
	}
	for(size_t i = 0; i < n; ++i) {
		// Sign-extend the 24-bit value.
		int32_t v = int32_t(bytes[3 * i] | bytes[3 * i + 1] << 8 | bytes[3 * i + 2] << 16);
		if(v & 0x800000) {
			v -= 0x1000000;
		}
		interleaved[i] = v / kScale;
	}
	return true;
}
