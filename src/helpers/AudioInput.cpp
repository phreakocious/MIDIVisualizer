#include "AudioInput.h"

#include <algorithm>
#include <cmath>
#include <utility>

#ifdef MIDIVIZ_SUPPORT_AUDIO_INPUT
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>

// Enough queued audio to ride out a stalled sink (a slow WAV write, a long frame).
static const int kBufferCount = 16;
static const double kBufferSeconds = 0.020;

struct InputDevice { AudioObjectID id; std::string name; int channels; };

// Element 0 is kAudioObjectPropertyElementMain, whose name needs the macOS 12 SDK.
static std::vector<InputDevice> inputDevices(){
	std::vector<InputDevice> list;
	AudioObjectPropertyAddress address = { kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, 0 };
	UInt32 size = 0;
	if(AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &address, 0, nullptr, &size) != noErr){
		return list;
	}
	std::vector<AudioObjectID> ids(size / sizeof(AudioObjectID));
	if(AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, nullptr, &size, ids.data()) != noErr){
		return list;
	}
	ids.resize(size / sizeof(AudioObjectID));

	for(AudioObjectID id : ids){
		AudioObjectPropertyAddress streams = { kAudioDevicePropertyStreamConfiguration, kAudioObjectPropertyScopeInput, 0 };
		if(AudioObjectGetPropertyDataSize(id, &streams, 0, nullptr, &size) != noErr || size == 0){
			continue;
		}
		std::vector<char> storage(size);
		AudioBufferList * buffers = reinterpret_cast<AudioBufferList *>(storage.data());
		if(AudioObjectGetPropertyData(id, &streams, 0, nullptr, &size, buffers) != noErr){
			continue;
		}
		int channels = 0;
		for(UInt32 b = 0; b < buffers->mNumberBuffers; ++b){
			channels += int(buffers->mBuffers[b].mNumberChannels);
		}
		if(channels == 0){
			continue;
		}

		AudioObjectPropertyAddress nameAddress = { kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal, 0 };
		CFStringRef name = nullptr;
		size = sizeof(name);
		if(AudioObjectGetPropertyData(id, &nameAddress, 0, nullptr, &size, &name) != noErr || name == nullptr){
			continue;
		}
		char utf8[256];
		const bool converted = CFStringGetCString(name, utf8, sizeof(utf8), kCFStringEncodingUTF8);
		CFRelease(name);
		if(converted){
			list.push_back({ id, utf8, channels });
		}
	}
	return list;
}
#endif

std::vector<AudioInput::Device> AudioInput::devices(){
	std::vector<Device> list;
#ifdef MIDIVIZ_SUPPORT_AUDIO_INPUT
	for(const InputDevice & device : inputDevices()){
		list.push_back({ device.name, device.channels });
	}
#endif
	return list;
}

double AudioInput::hostNow(){
#ifdef MIDIVIZ_SUPPORT_AUDIO_INPUT
	return double(AudioConvertHostTimeToNanos(AudioGetCurrentHostTime())) * 1e-9;
#else
	return 0.0;
#endif
}

AudioInput::~AudioInput(){
	close();
}

bool AudioInput::open(const std::string & deviceName, int firstChannel){
	close();
	_error.clear();
#ifdef MIDIVIZ_SUPPORT_AUDIO_INPUT
	const std::vector<InputDevice> list = inputDevices();
	const auto device = std::find_if(list.begin(), list.end(), [&deviceName](const InputDevice & d){ return d.name == deviceName; });
	if(device == list.end()){
		_error = "No audio input named " + deviceName + ".";
		return false;
	}
	if(firstChannel < 1 || firstChannel + 1 > device->channels){
		_error = "The device has only " + std::to_string(device->channels) + " inputs.";
		return false;
	}

	AudioObjectPropertyAddress rateAddress = { kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, 0 };
	AudioObjectPropertyAddress uidAddress = { kAudioDevicePropertyDeviceUID, kAudioObjectPropertyScopeGlobal, 0 };
	Float64 rate = 0.0;
	CFStringRef uid = nullptr;
	UInt32 rateSize = sizeof(rate);
	UInt32 uidSize = sizeof(uid);
	if(AudioObjectGetPropertyData(device->id, &rateAddress, 0, nullptr, &rateSize, &rate) != noErr || rate <= 0.0
	   || AudioObjectGetPropertyData(device->id, &uidAddress, 0, nullptr, &uidSize, &uid) != noErr || uid == nullptr){
		_error = "Unable to read the settings of " + deviceName + ".";
		return false;
	}

	// All the device's inputs at its own rate, so the queue neither resamples nor remixes.
	AudioStreamBasicDescription format = {};
	format.mSampleRate = rate;
	format.mFormatID = kAudioFormatLinearPCM;
	format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
	format.mBitsPerChannel = 32;
	format.mChannelsPerFrame = UInt32(device->channels);
	format.mFramesPerPacket = 1;
	format.mBytesPerFrame = 4 * format.mChannelsPerFrame;
	format.mBytesPerPacket = format.mBytesPerFrame;

	_channels = device->channels;
	_first = firstChannel;
	_rate = int(std::lround(rate));
	_nextSample = -1.0;
	_dropped = 0;
	{
		std::lock_guard<std::mutex> lock(_mutex);
		_peak = 0.0f;
	}

	const AudioQueueInputCallback onBuffer = [](void * user, AudioQueueRef queue, AudioQueueBufferRef buffer, const AudioTimeStamp * start, UInt32, const AudioStreamPacketDescription *){
		AudioInput * self = static_cast<AudioInput *>(user);
		self->deliver(static_cast<const float *>(buffer->mAudioData), buffer->mAudioDataByteSize / (4 * UInt32(self->_channels)), start->mSampleTime, start->mHostTime);
		// Fails harmlessly while the queue stops.
		AudioQueueEnqueueBuffer(queue, buffer, 0, nullptr);
	};

	AudioQueueRef queue = nullptr;
	OSStatus status = AudioQueueNewInput(&format, onBuffer, this, nullptr, nullptr, 0, &queue);
	if(status == noErr){
		status = AudioQueueSetProperty(queue, kAudioQueueProperty_CurrentDevice, &uid, sizeof(uid));
	}
	CFRelease(uid);
	const UInt32 bufferBytes = UInt32(std::lround(rate * kBufferSeconds)) * format.mBytesPerFrame;
	for(int i = 0; i < kBufferCount && status == noErr; ++i){
		AudioQueueBufferRef buffer = nullptr;
		status = AudioQueueAllocateBuffer(queue, bufferBytes, &buffer);
		if(status == noErr){
			status = AudioQueueEnqueueBuffer(queue, buffer, 0, nullptr);
		}
	}
	if(status == noErr){
		status = AudioQueueStart(queue, nullptr);
	}
	if(status != noErr){
		if(queue != nullptr){
			AudioQueueDispose(queue, true);
		}
		_error = "Unable to capture from " + deviceName + " (error " + std::to_string(status) + ").";
		return false;
	}
	_queue = queue;
	return true;
#else
	(void)deviceName;
	(void)firstChannel;
	_error = "This build cannot capture audio.";
	return false;
#endif
}

#ifdef MIDIVIZ_SUPPORT_AUDIO_INPUT
void AudioInput::deliver(const float * interleaved, size_t frames, double sampleTime, uint64_t hostTime){
	if(frames == 0){
		return;
	}
	const double pts = double(AudioConvertHostTimeToNanos(hostTime)) * 1e-9;
	// The sample count jumps when the queue loses audio: fill the hole so later samples keep their time.
	const int64_t gap = _nextSample < 0.0 ? 0 : int64_t(std::llround(sampleTime - _nextSample));
	_nextSample = sampleTime + double(frames);

	const size_t channels = size_t(_channels);
	const float * pair = interleaved + (_first - 1);
	_stereo.resize(2 * frames);
	float peak = 0.0f;
	for(size_t f = 0; f < frames; ++f){
		_stereo[2 * f] = pair[f * channels];
		_stereo[2 * f + 1] = pair[f * channels + 1];
		peak = std::max(peak, std::max(std::abs(_stereo[2 * f]), std::abs(_stereo[2 * f + 1])));
	}

	std::lock_guard<std::mutex> lock(_mutex);
	_peak = std::max(_peak, peak);
	if(gap > 0){
		_dropped += gap;
		if(_sink){
			const std::vector<float> silence(size_t(gap) * 2, 0.0f);
			_sink(pts - double(gap) / double(_rate), silence.data(), size_t(gap));
		}
	}
	if(_sink){
		_sink(pts, _stereo.data(), frames);
	}
}
#endif

void AudioInput::close(){
#ifdef MIDIVIZ_SUPPORT_AUDIO_INPUT
	if(_queue != nullptr){
		// Both immediate: the stop returns once stopped, and no callback runs after the dispose returns.
		AudioQueueStop(_queue, true);
		AudioQueueDispose(_queue, true);
		_queue = nullptr;
	}
#endif
}

bool AudioInput::isOpen() const {
	return _queue != nullptr;
}

const std::string & AudioInput::error() const {
	return _error;
}

int AudioInput::rate() const {
	return _rate;
}

float AudioInput::takeLevel(){
	std::lock_guard<std::mutex> lock(_mutex);
	const float peak = std::min(_peak, 1.0f);
	_peak = 0.0f;
	return peak;
}

int64_t AudioInput::droppedFrames() const {
	return _dropped;
}

void AudioInput::setSink(std::function<void(double ptsHost, const float * stereo, size_t frames)> sink){
	std::lock_guard<std::mutex> lock(_mutex);
	_sink = std::move(sink);
}
