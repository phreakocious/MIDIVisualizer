#pragma once
#include <gl3w/gl3w.h>
#include <glm/glm.hpp>

#include "../midi/MIDIFile.h"
#include "State.h"
#include "../helpers/ProgramUtilities.h"

#include <array>
#include <fstream>
#include <memory>

class MIDIScene;

class Renderer {

public:

	void renderSetup();

	void upload(const std::shared_ptr<MIDIScene>& scene);

	void setScaleAndMinorWidth(const float scale, const float minorWidth);

	void setParticlesParameters(const float speed, const float expansion);

	void setKeyboardSizeAndFadeout(float keyboardHeight, float fadeOut);

	void setMinorEdgesAndHeight(bool minorEdges, float minorHeight);

	void setMinMaxKeys(int minKey, int minKeyMajor, int notesCount);

	void setOrientation(bool horizontal);

	/// Draw function
	void drawNotes(const std::shared_ptr<MIDIScene>& scene, float time, const glm::vec2 & invScreenSize, const State::NotesState & state, bool reverseScroll, bool prepass);
	
	void drawFlashes(const std::shared_ptr<MIDIScene>& scene, float time, const glm::vec2 & invScreenSize, const State::FlashesState& state);
	
	void drawParticles(const std::shared_ptr<MIDIScene>& scene, float time, const glm::vec2 & invScreenSize, const State::ParticlesState & state, bool prepass);
	
	void drawKeyboard(const std::shared_ptr<MIDIScene>& scene, float time, const glm::vec2 & invScreenSize, const glm::vec3 & edgeColor, const glm::vec3 & keyColor, const ColorArray & majorColors, const ColorArray & minorColors, bool highlightKeys);

	/// Keyboard as lit 3D keys on the floor of the 3D view, from the front edge (z = 0) back to z = -depth.
	/// Keys ease down and up over real time, so time must be the playback clock.
	void drawKeyboard3D(const std::shared_ptr<MIDIScene>& scene, float time, const glm::mat4 & mvp, const glm::vec3 & eye, float aspect, float depth, const glm::vec3 & whiteColor, const glm::vec3 & blackColor, const ColorArray & majorColors, const ColorArray & minorColors, bool highlightKeys, float minorsWidth, float minorHeight);

	void drawPedals(const std::shared_ptr<MIDIScene>& scene, float time, const glm::vec2 & invScreenSize, const State::PedalsState & state, float keyboardHeight, bool horizontalMode);

	void drawWaves(const std::shared_ptr<MIDIScene>& scene, float time, const glm::vec2 & invScreenSize, const State::WaveState & state, float keyboardHeight);

	void drawScore(const std::shared_ptr<MIDIScene>& scene, float time, const glm::vec2 & invScreenSize, const State::ScoreState & state, float measureScale, float qualityScale,  float keyboardHeight, bool horizontalMode, bool reverseScroll);

	void clean();

private:
	
	ShaderProgram _programNotes;
	ShaderProgram _programFlashes;
	ShaderProgram _programParticules;
	ShaderProgram _programKeyMinors;
	ShaderProgram _programKeyMajors;
	ShaderProgram _programKeys3D;
	ShaderProgram _programPedals;
	ShaderProgram _programWave;
	ShaderProgram _programWaveNoise;
	ShaderProgram _programScoreBars;
	ShaderProgram _programScoreLabels;

	GLuint _notesDataBuffer;
	GLuint _keysDataBuffer;
	GLuint _keysGainBuffer;
	GLuint _quadVertices;
	GLuint _quadIndices;
	GLuint _waveVertices;
	GLuint _waveIndices;

	GLuint _vaoQuad;
	GLuint _vaoQuadWithNoteData;
	GLuint _vaoQuadWithKeyData;
	size_t _quadPrimitiveCount;

	GLuint _vaoWave;
	size_t _wavePrimitiveCount;

	GLuint _vaoKeys3D;
	GLuint _keys3DVertices;
	GLsizei _keys3DVertexCount = 0;
	std::array<float, 6> _keys3DLayout{}; ///< Layout the keyboard mesh was built for.
	std::array<float, 128> _keys3DPress{}; ///< How far each key is down, 0 to 1, before easing.
	std::array<int, 128> _keys3DLastSet{}; ///< Color set of each key's last note, shown while it comes back up.
	float _keys3DTime = 0.0f;
	GLuint _keysShadowMap;
	GLuint _keysShadowFramebuffer;

	GLuint _texParticles;
	GLuint _texFont;
	GLuint _texNoise;

	// Cached info.
	unsigned int _minKeyMajor{0};
	unsigned int _keyCount{128};
};
