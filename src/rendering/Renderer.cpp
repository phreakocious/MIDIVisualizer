#include <stdio.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <array>
#include <cassert>
#include <glm/gtc/matrix_transform.hpp>

#include "../helpers/ResourcesManager.h"

#include "Renderer.h"
#include "scene/MIDIScene.h"

#include <imgui/imgui.h>

#ifdef _WIN32
#undef MIN
#undef MAX
#endif


// Size of the 3D keyboard's shadow map: the keyboard is wide and shallow.
static const glm::ivec2 keysShadowSize(4096, 1024);

void Renderer::renderSetup(){
	// - Buffers

	// -- Basic quad
	std::vector<float> vertices = {-0.5,-0.5, 0.5, -0.5, 0.5,0.5, -0.5, 0.5};
	std::vector<unsigned int> indices = {1, 2, 0, 2, 3, 0};

	// Create an array buffer to host the geometry data.

	glGenBuffers(1, &_quadVertices);
	glBindBuffer(GL_ARRAY_BUFFER, _quadVertices);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * vertices.size(), &(vertices[0]), GL_STATIC_DRAW);

	// We load the indices data
	glGenBuffers(1, &_quadIndices);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _quadIndices);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * indices.size(), &(indices[0]), GL_STATIC_DRAW);
	_quadPrimitiveCount = indices.size();

	// -- Notes data
	// Current notes on screen
	_notesDataBuffer = 0;
	glGenBuffers(1, &_notesDataBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, _notesDataBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 1000, nullptr, GL_STATIC_DRAW);

	// Active keys buffer (empty for now).
	_keysDataBuffer = 0;
	glGenBuffers(1, &_keysDataBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, _keysDataBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(int) * 128, nullptr, GL_DYNAMIC_DRAW);
	// Velocity gain of each active key.
	_keysGainBuffer = 0;
	glGenBuffers(1, &_keysGainBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, _keysGainBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 128, nullptr, GL_DYNAMIC_DRAW);

	// -- Wave strips
	const int numSegments = 512;
	std::vector<glm::vec2> waveVerts((numSegments+1)*2);
	for(int vid = 0; vid < numSegments+1; ++vid){
		const float x = 2.0f * float(vid) / float(numSegments) - 1.0f;
		waveVerts[2*vid] = glm::vec2(x, 1.0f);
		waveVerts[2*vid+1] = glm::vec2(x, -1.0f);
	}
	std::vector<unsigned int> waveInds(3 * 2 * numSegments);
	for(int sid = 0; sid < numSegments; ++sid){
		const int bid = 6 * sid;
		const int vid = 2 * sid;
		waveInds[bid  ] = vid;
		waveInds[bid+1] = vid + 1;
		waveInds[bid+2] = vid + 2;
		waveInds[bid+3] = vid + 2;
		waveInds[bid+4] = vid + 1;
		waveInds[bid+5] = vid + 3;
	}

	glGenBuffers(1, &_waveVertices);
	glBindBuffer(GL_ARRAY_BUFFER, _waveVertices);
	glBufferData(GL_ARRAY_BUFFER, 2 * sizeof(GLfloat) * waveVerts.size(), &(waveVerts[0][0]), GL_STATIC_DRAW);
	// Corresponding indices.
	glGenBuffers(1, &_waveIndices);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _waveIndices);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * waveInds.size(), &(waveInds[0]), GL_STATIC_DRAW);
	_wavePrimitiveCount = waveInds.size();

	// Reset state
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// - Vertex arrays

	// -- Basic quad
	glGenVertexArrays(1, &_vaoQuad);
	glBindVertexArray(_vaoQuad);
	// Positions
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, _quadVertices);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	glVertexAttribDivisor(0, 0);
	// Indices
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _quadIndices);
	glBindVertexArray(0);

	// -- Quad with notes data.
	glGenVertexArrays(1, &_vaoQuadWithNoteData);
	glBindVertexArray(_vaoQuadWithNoteData);
	// Positions
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, _quadVertices);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	glVertexAttribDivisor(0, 0);
	// Notes data part 1
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, _notesDataBuffer);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), NULL);
	glVertexAttribDivisor(1, 1);
	// Notes data part 2
	glEnableVertexAttribArray(2);
	glBindBuffer(GL_ARRAY_BUFFER, _notesDataBuffer);
	glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)(4 * sizeof(GLfloat)));
	glVertexAttribDivisor(2, 1);
	// Notes data part 3: velocity gain
	glEnableVertexAttribArray(3);
	glBindBuffer(GL_ARRAY_BUFFER, _notesDataBuffer);
	glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)(5 * sizeof(GLfloat)));
	glVertexAttribDivisor(3, 1);
	// Indices
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _quadIndices);
	glBindVertexArray(0);

	// -- Quad with active keys data.
	glGenVertexArrays(1, &_vaoQuadWithKeyData);
	glBindVertexArray(_vaoQuadWithKeyData);
	// Positions
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, _quadVertices);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	glVertexAttribDivisor(0, 0);
	// Keys data
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, _keysDataBuffer);
	glVertexAttribIPointer(1, 1, GL_INT, 0, NULL);
	glVertexAttribDivisor(1, 1);
	// Keys velocity gain
	glEnableVertexAttribArray(2);
	glBindBuffer(GL_ARRAY_BUFFER, _keysGainBuffer);
	glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 0, NULL);
	glVertexAttribDivisor(2, 1);
	// Indices
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _quadIndices);
	glBindVertexArray(0);

	// -- Wave
	glGenVertexArrays(1, &_vaoWave);
	glBindVertexArray(_vaoWave);
	// Positions
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, _waveVertices);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	glVertexAttribDivisor(0, 0);
	// Indices
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _waveIndices);
	glBindVertexArray(0);

	// -- 3D keyboard: one mesh for all keys and the case, built in drawKeyboard3D when the layout changes.
	// Each vertex is a position, a normal, and its MIDI note (-1 for the case).
	glGenBuffers(1, &_keys3DVertices);
	glGenVertexArrays(1, &_vaoKeys3D);
	glBindVertexArray(_vaoKeys3D);
	glBindBuffer(GL_ARRAY_BUFFER, _keys3DVertices);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(GLfloat), NULL);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 7 * sizeof(GLfloat), (void*)(6 * sizeof(GLfloat)));
	glBindVertexArray(0);

	// The keys' shadow map: depth seen from the light, compared on lookup.
	glGenTextures(1, &_keysShadowMap);
	glBindTexture(GL_TEXTURE_2D, _keysShadowMap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, keysShadowSize.x, keysShadowSize.y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
	glBindTexture(GL_TEXTURE_2D, 0);
	glGenFramebuffers(1, &_keysShadowFramebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, _keysShadowFramebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _keysShadowMap, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	checkGLError();

	// Programs.
	_programNotes.init("notes_vert", "notes_frag");
	_programFlashes.init("flashes_vert", "flashes_frag");
	_programParticules.init("particles_vert", "particles_frag");
	_programKeyMajors.init("majorKeys_vert", "majorKeys_frag");
	_programKeyMinors.init("minorKeys_vert", "minorKeys_frag");
	_programKeys3D.init("keys3d_vert", "keys3d_frag");
	_programPedals.init("pedal_vert", "pedal_frag");
	_programScoreBars.init("score_bars_vert", "score_bars_frag");
	_programScoreLabels.init("score_labels_vert", "score_labels_frag");
	_programWave.init("wave_vert", "wave_frag");
	_programWaveNoise.init("wave_noise_vert", "wave_noise_frag");

	// Pass texture size to shader.
	_texFont = ResourcesManager::getTextureFor("font");
	_texNoise = ResourcesManager::getTextureFor("noise");
	_texParticles = ResourcesManager::getTextureFor("particles");
	const glm::vec2 tsize = ResourcesManager::getTextureSizeFor("particles");
	_programParticules.use();
	_programParticules.uniform("inverseTextureSize", 1.0f / tsize);
	glUseProgram(0);

}

void Renderer::upload(const std::shared_ptr<MIDIScene>& scene){
	glm::ivec2 uploadRange;
	if(scene->dirtyNotes(uploadRange)){
		constexpr size_t noteSize = sizeof(MIDIScene::GPUNote);
		const auto& notes = scene->getNotes();
		glBindBuffer(GL_ARRAY_BUFFER, _notesDataBuffer);
		if(uploadRange.y == 0){ // Full vector
			glBufferData(GL_ARRAY_BUFFER, noteSize * notes.size(), &(notes[0]), GL_DYNAMIC_DRAW);
		} else {
			const int size = uploadRange.y - uploadRange.x + 1;
			const int first = uploadRange.x;
			glBufferSubData(GL_ARRAY_BUFFER, first * noteSize, size * noteSize, &(notes[first]));
		}
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		scene->setUpToDate();
	}

	// Update the flags buffer accordingly.
	const auto& actives = scene->getActiveKeys();
	glBindBuffer(GL_ARRAY_BUFFER, _keysDataBuffer);
	glBufferSubData(GL_ARRAY_BUFFER, 0, actives.size()*sizeof(int) ,&(actives[0]));
	const auto& gains = scene->getActiveGains();
	glBindBuffer(GL_ARRAY_BUFFER, _keysGainBuffer);
	glBufferSubData(GL_ARRAY_BUFFER, 0, gains.size()*sizeof(GLfloat), &(gains[0]));

	_programKeyMajors.use();
	_programKeyMajors.uniforms("actives", actives.size(), actives.data());
	_programKeyMinors.use();
	_programKeyMinors.uniforms("actives", actives.size(), actives.data());
	glUseProgram(0);
}

void Renderer::setScaleAndMinorWidth(const float scale, const float minorWidth){
	_programNotes.use();
	_programNotes.uniform("mainSpeed", scale);
	_programNotes.uniform("minorsWidth", minorWidth);
	_programKeyMinors.use();
	_programKeyMinors.uniform("minorsWidth", minorWidth);
	glUseProgram(0);
}

void Renderer::setParticlesParameters(const float speed, const float expansion){
	_programParticules.use();
	_programParticules.uniform("speedScaling", speed);
	_programParticules.uniform("expansionFactor", expansion);
	glUseProgram(0);
}

void Renderer::setKeyboardSizeAndFadeout(float keyboardHeight, float fadeOut){
	const float fadeOutFinal = keyboardHeight + (1.0f - keyboardHeight) * (1.0f - fadeOut);

	_programNotes.use();
	_programNotes.uniform("keyboardHeight", keyboardHeight);
	_programNotes.uniform("fadeOut", fadeOutFinal);
	_programParticules.use();
	_programParticules.uniform("keyboardHeight", keyboardHeight);
	_programKeyMinors.use();
	_programKeyMinors.uniform("keyboardHeight", keyboardHeight);
	_programKeyMajors.use();
	_programKeyMajors.uniform("keyboardHeight", keyboardHeight);
	_programFlashes.use();
	_programFlashes.uniform("keyboardHeight", keyboardHeight);
	glUseProgram(0);
}

void Renderer::setMinorEdgesAndHeight(bool minorEdges, float minorHeight){
	_programKeyMinors.use();
	_programKeyMinors.uniform("edgeOnMinors", minorEdges);
	_programKeyMinors.uniform("minorsHeight", minorHeight);
	glUseProgram(0);
}

void Renderer::setMinMaxKeys(int minKey, int minKeyMajor, int notesCount){
	_programNotes.use();
	_programNotes.uniform("minNoteMajor", minKeyMajor);
	_programNotes.uniform("notesCount", float(notesCount));
	_programFlashes.use();
	_programFlashes.uniform("minNote", minKey);
	_programFlashes.uniform("notesCount", float(notesCount));
	_programKeyMajors.use();
	_programKeyMajors.uniform("minNoteMajor", minKeyMajor);
	_programKeyMajors.uniform("notesCount", float(notesCount));
	_programKeyMinors.use();
	_programKeyMinors.uniform("minNoteMajor", minKeyMajor);
	_programKeyMinors.uniform("notesCount", float(notesCount));
	_programParticules.use();
	_programParticules.uniform("minNote", minKey);
	_programParticules.uniform("notesCount", float(notesCount));
	// Refresh score settings (used on CPU)
	_minKeyMajor = minKeyMajor;
	_keyCount = notesCount;
	glUseProgram(0);
}

void Renderer::setOrientation(bool horizontal){
	_programNotes.use();
	_programNotes.uniform("horizontalMode", horizontal);
	_programFlashes.use();
	_programFlashes.uniform("horizontalMode", horizontal);
	_programKeyMajors.use();
	_programKeyMajors.uniform("horizontalMode", horizontal);
	_programKeyMinors.use();
	_programKeyMinors.uniform("horizontalMode", horizontal);
	_programParticules.use();
	_programParticules.uniform("horizontalMode", horizontal);
	_programWave.use();
	_programWave.uniform("horizontalMode", horizontal);
	_programWaveNoise.use();
	_programWaveNoise.uniform("horizontalMode", horizontal);
	_programScoreBars.use();
	_programScoreBars.uniform("horizontalMode", horizontal);
	_programScoreLabels.use();
	_programScoreLabels.uniform("horizontalMode", horizontal);
	glUseProgram(0);
}

void Renderer::drawParticles(const std::shared_ptr<MIDIScene>& scene, float time, const glm::vec2 & invScreenSize, const State::ParticlesState & state, bool prepass){

	glEnable(GL_BLEND);
	_programParticules.use();
	
	// Common uniforms values.
	_programParticules.uniform("inverseScreenSize", invScreenSize);
	_programParticules.uniform("time", 0.0f);

	// Prepass : bigger, darker particles.
	_programParticules.uniform("colorScale", prepass ? 0.6f : 1.6f);
	_programParticules.uniform("scale", state.scale * (prepass ? 2.0f : 1.0f));

	_programParticules.uniforms("baseColor", state.colors.size(), state.colors.data());
	
	// Particles trajectories texture.
	_programParticules.texture("textureParticles", _texParticles, GL_TEXTURE_2D);
	_programParticules.texture("textureNoise", _texNoise, GL_TEXTURE_2D);
	_programParticules.texture("lookParticles", state.tex, GL_TEXTURE_2D_ARRAY);
	_programParticules.uniform("turbulenceStrength", state.turbulenceStrength);
	_programParticules.uniform("turbulenceScale", state.turbulenceScale);
	_programParticules.uniform("texCount", state.texCount);

	// Select the geometry.
	glBindVertexArray(_vaoQuad);
	// For each active particles system, draw it with the right parameters.
	for(const auto & particle : scene->getParticles()){
		if(particle.note >= 0){
			_programParticules.uniform("globalId", particle.note);
			_programParticules.uniform("time", particle.elapsed);
			_programParticules.uniform("duration", particle.duration);
			_programParticules.uniform("channel", particle.set);
			// Louder notes throw more particles, further.
			const float gain = glm::mix(1.0f, particle.gain, state.velocityStrength);
			_programParticules.uniform("expansionFactor", state.expansion * gain);
			// At least one particle, so soft notes still emit with a small count.
			const int count = state.count < 1 ? state.count : (std::max)(1, int(std::round(float(state.count) * gain)));
			glDrawElementsInstanced(GL_TRIANGLES, int(_quadPrimitiveCount), GL_UNSIGNED_INT, (void*)0, count);
		}
	}
	
	glBindVertexArray(0);
	glUseProgram(0);
	glDisable(GL_BLEND);

}

void Renderer::drawNotes(const std::shared_ptr<MIDIScene>& scene, float time, const glm::vec2 & invScreenSize, const State::NotesState & state, bool reverseScroll, bool prepass){
	
	_programNotes.use();
	
	// Uniforms setup.
	const float maxCornerRadius = 0.12f; // Internal tweaking to avoid notes whose edges are not timing-aligned.

	_programNotes.uniform("inverseScreenSize", invScreenSize);
	_programNotes.uniform("time", time);
	_programNotes.uniform("colorScale", prepass ? 0.6f: 1.0f);

	_programNotes.uniforms("baseColor" , state.majorColors.size(), state.majorColors.data());
	_programNotes.uniforms("minorColor", state.minorColors.size(), state.minorColors.data());
	_programNotes.uniform("reverseMode", reverseScroll);
	_programNotes.uniform("edgeWidth", state.edgeWidth);
	_programNotes.uniform("edgeBrightness", state.edgeBrightness);
	_programNotes.uniform("velocityStrength", state.velocityStrength);

	_programNotes.uniform("texturesScale", glm::vec2{state.majorTexScale, state.minorTexScale});
	_programNotes.uniform("texturesStrength", glm::vec2{state.majorTexAlpha, state.minorTexAlpha});
	_programNotes.uniform("scrollMajorTexture", state.majorTexScroll);
	_programNotes.uniform("scrollMinorTexture", state.minorTexScroll);
	_programNotes.uniform("cornerRadius", state.cornerRadius * maxCornerRadius);

	// Pass note textures if present, and corresponding flags.
	_programNotes.uniform("useMajorTexture", state.majorTex != 0);
	_programNotes.uniform("useMinorTexture", state.minorTex != 0);
	// IDs below can be 0
	_programNotes.texture("majorTexture", state.majorTex, GL_TEXTURE_2D);
	_programNotes.texture("minorTexture", state.minorTex, GL_TEXTURE_2D);
	
	// Draw the geometry.
	glBindVertexArray(_vaoQuadWithNoteData);
	glDrawElementsInstanced(GL_TRIANGLES, int(_quadPrimitiveCount), GL_UNSIGNED_INT, (void*)0, GLsizei(scene->getEffectiveNotesCount()));

	glBindVertexArray(0);
	glUseProgram(0);
	
}

void Renderer::drawFlashes(const std::shared_ptr<MIDIScene>& scene, float time, const glm::vec2 & invScreenSize, const State::FlashesState& state){
	
	// Need alpha blending.
	glEnable(GL_BLEND);

	glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ONE);
	
	_programFlashes.use();
	
	// Uniforms setup.
	_programFlashes.uniform("inverseScreenSize", invScreenSize);
	_programFlashes.uniform("time",time);
	_programFlashes.uniforms("baseColor", state.colors.size(), state.colors.data());
	_programFlashes.uniform("userScale", state.size);
	_programFlashes.uniform("velocityStrength", state.velocityStrength);
	_programFlashes.uniform("haloInnerRadius", state.haloInnerRadius);
	_programFlashes.uniform("haloOuterRadius", state.haloOuterRadius);
	_programFlashes.uniform("haloIntensity", state.haloIntensity);
	_programFlashes.uniform("texRowCount", state.texRowCount);
	_programFlashes.uniform("texColCount", state.texColCount);
	// Flash texture.
	_programFlashes.texture("textureFlash", state.tex, GL_TEXTURE_2D);
	
	// Draw the geometry.
	glBindVertexArray(_vaoQuadWithKeyData);
	glDrawElementsInstanced(GL_TRIANGLES, int(_quadPrimitiveCount), GL_UNSIGNED_INT, (void*)0, 128);
	
	glBindVertexArray(0);
	glUseProgram(0);
	glDisable(GL_BLEND);

	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
}

void Renderer::drawKeyboard(const std::shared_ptr<MIDIScene>& scene, float, const glm::vec2 & invScreenSize, const glm::vec3 & edgeColor, const glm::vec3 & keyColor, const ColorArray & majorColors, const ColorArray & minorColors, bool highlightKeys) {

	{
		_programKeyMajors.use();
		// Uniforms setup.
		_programKeyMajors.uniform("inverseScreenSize", invScreenSize);
		_programKeyMajors.uniform("edgeColor", edgeColor);
		_programKeyMajors.uniform("keyColor", keyColor);
		_programKeyMajors.uniforms("majorColor", majorColors.size(), majorColors.data());
		_programKeyMajors.uniform("highlightKeys", highlightKeys);

		// Draw the geometry.
		glBindVertexArray(_vaoQuad);
		glDrawElements(GL_TRIANGLES, int(_quadPrimitiveCount), GL_UNSIGNED_INT, (void*)0);
	}

	{
		_programKeyMinors.use();
		// Uniforms setup.
		_programKeyMinors.uniform("inverseScreenSize", invScreenSize);
		_programKeyMinors.uniform("edgeColor", edgeColor);
		_programKeyMinors.uniforms("minorColor", minorColors.size(), minorColors.data());
		_programKeyMinors.uniform("highlightKeys", highlightKeys);

		glBindVertexArray(_vaoQuad);
		glDrawElementsInstanced(GL_TRIANGLES, int(_quadPrimitiveCount), GL_UNSIGNED_INT, (void*)0, 53);
	}
	
	glBindVertexArray(0);
	glUseProgram(0);
}

namespace {

// Outward unit normal of each edge of an outline in (x, z); edge i runs from o[i] to o[i+1].
std::vector<glm::vec2> outwardNormals(const std::vector<glm::vec2>& o){
	const size_t n = o.size();
	float area = 0.0f;
	for(size_t i = 0; i < n; ++i){
		area += o[i].x * o[(i + 1) % n].y - o[(i + 1) % n].x * o[i].y;
	}
	std::vector<glm::vec2> res(n);
	for(size_t i = 0; i < n; ++i){
		const glm::vec2 e = glm::normalize(o[(i + 1) % n] - o[i]);
		res[i] = (area > 0.0f ? 1.0f : -1.0f) * glm::vec2(e.y, -e.x);
	}
	return res;
}

// Moves each edge of an outline inward by its own distance d[i]. Holds only where consecutive edges are perpendicular.
std::vector<glm::vec2> insetOutline(const std::vector<glm::vec2>& o, const std::vector<glm::vec2>& normals, const std::vector<float>& d){
	const size_t n = o.size();
	std::vector<glm::vec2> res(n);
	for(size_t i = 0; i < n; ++i){
		const size_t prev = (i + n - 1) % n;
		assert(std::abs(glm::dot(normals[prev], normals[i])) < 1e-4f);
		res[i] = o[i] - d[prev] * normals[prev] - d[i] * normals[i];
	}
	return res;
}

void addVertex(std::vector<GLfloat>& out, const glm::vec3& p, const glm::vec3& n, float note){
	out.insert(out.end(), {p.x, p.y, p.z, n.x, n.y, n.z, note});
}

void addQuad(std::vector<GLfloat>& out, const glm::vec3 (&p)[4], const glm::vec3 (&n)[4], float note){
	for(int i : {0, 1, 2, 0, 2, 3}){
		addVertex(out, p[i], n[i], note);
	}
}

// A key or a block of the case. The outline in (x, z) rises from y0 in walls that lean in by taper[i] per edge,
// then a bevel of width r whose normals turn from the wall's to straight up, so the top edge reads as rounded.
// The top is a fan from c, which must see the whole top outline.
void addBlock(std::vector<GLfloat>& out, const std::vector<glm::vec2>& o, const std::vector<float>& taper, float r, float y0, float h, const glm::vec2& c, float note){
	const size_t n = o.size();
	const std::vector<glm::vec2> normals = outwardNormals(o);
	std::vector<float> topInset(taper);
	for(float& t : topInset){
		t += r;
	}
	const std::vector<glm::vec2> mid = insetOutline(o, normals, taper);
	const std::vector<glm::vec2> top = insetOutline(o, normals, topInset);
	const glm::vec3 up(0.0f, 1.0f, 0.0f);
	const float wallHeight = h - r - y0;
	for(size_t i = 0; i < n; ++i){
		const size_t j = (i + 1) % n;
		const glm::vec3 wn = glm::normalize(glm::vec3(normals[i].x * wallHeight, taper[i], normals[i].y * wallHeight));
		const glm::vec3 b0(o[i].x, y0, o[i].y), b1(o[j].x, y0, o[j].y);
		const glm::vec3 m0(mid[i].x, h - r, mid[i].y), m1(mid[j].x, h - r, mid[j].y);
		const glm::vec3 t0(top[i].x, h, top[i].y), t1(top[j].x, h, top[j].y);
		addQuad(out, {b0, b1, m1, m0}, {wn, wn, wn, wn}, note);
		addQuad(out, {m0, m1, t1, t0}, {wn, wn, up, up}, note);
		addVertex(out, glm::vec3(c.x, h, c.y), up, note);
		addVertex(out, t0, up, note);
		addVertex(out, t1, up, note);
	}
}

}

void Renderer::drawKeyboard3D(const std::shared_ptr<MIDIScene>& scene, float time, const glm::mat4 & mvp, const glm::vec3 & eye, float aspect, float depth, const glm::vec3 & whiteColor, const glm::vec3 & blackColor, const ColorArray & majorColors, const ColorArray & minorColors, bool highlightKeys, float minorsWidth, float minorHeight) {
	// Sizes follow a real piano, in white key widths ("slots", 23.5 mm). The x layout is the flat keyboard's, so the notes line up.
	const float slot = 2.0f * aspect / float(_keyCount);
	const float gap = 0.05f * slot;
	const float whiteTop = 0.7f * slot;
	const float blackTop = whiteTop + 0.5f * slot;
	const float blackWidth = 0.9f * slot * minorsWidth;
	const float blackFront = -depth + minorHeight * depth;
	// A key pivots on the balance rail, well behind its visible part; its front goes down 10 mm.
	const float pivotZ = -1.5f * depth;
	const float pressAngle = 0.42f * slot / -pivotZ;
	// The key slip in front hides a pressed key's front; the key bed fills the gaps.
	const float slipTop = 0.2f * slot;
	const float slipBottom = -1.5f * slot;
	const float slipFront = gap + 0.6f * slot;
	const float bedY = -0.5f * slot;

	const std::array<float, 6> layout = {{aspect, depth, float(_keyCount), float(_minKeyMajor), minorsWidth, minorHeight}};
	if(layout != _keys3DLayout){
		_keys3DLayout = layout;
		static const int whiteIndex[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};
		static const bool isBlack[12] = {false, true, false, true, false, false, true, false, true, false, true, false};
		// Black keys first: white keys are notched around the ones that are shown.
		std::array<float, 128> blackX;
		std::array<bool, 128> shown;
		for(int note = 0; note < 128; ++note){
			const int k = note % 12;
			const int white = (note / 12) * 7 + whiteIndex[k] - int(_minKeyMajor);
			if(!isBlack[k]){
				shown[note] = white >= 0 && white < int(_keyCount);
				continue;
			}
			// Between this white key and the next, nudged like the flat keyboard.
			const float shift = (k == 1 || k == 6) ? -0.1f : ((k == 3 || k == 10) ? 0.1f : 0.0f);
			blackX[note] = -aspect + float(white + 1) * slot + shift * blackWidth;
			shown[note] = std::abs(blackX[note]) < aspect - 0.5f * slot;
		}
		std::vector<GLfloat> mesh;
		for(int note = 0; note < 128; ++note){
			if(!shown[note]){
				continue;
			}
			const int k = note % 12;
			if(isBlack[k]){
				const float x0 = blackX[note] - 0.5f * blackWidth;
				const float x1 = blackX[note] + 0.5f * blackWidth;
				// Sides narrow toward the top; the front slopes back.
				const std::vector<glm::vec2> outline = {{x0, -depth}, {x1, -depth}, {x1, blackFront}, {x0, blackFront}};
				const std::vector<float> taper = {0.0f, 0.1f * slot, 0.3f * slot, 0.1f * slot};
				addBlock(mesh, outline, taper, 0.1f * slot, 0.0f, blackTop, {blackX[note], 0.5f * (blackFront - depth)}, float(note));
				continue;
			}
			const int white = (note / 12) * 7 + whiteIndex[k] - int(_minKeyMajor);
			const float x0 = -aspect + float(white) * slot + 0.5f * gap;
			const float x1 = x0 + slot - gap;
			// Behind the notch, the key narrows to the space between its black neighbours.
			const float notchZ = blackFront + gap;
			const float b0 = (note > 0 && shown[note - 1] && isBlack[(note - 1) % 12]) ? blackX[note - 1] + 0.5f * blackWidth + gap : x0;
			const float b1 = (note < 127 && shown[note + 1] && isBlack[(note + 1) % 12]) ? blackX[note + 1] - 0.5f * blackWidth - gap : x1;
			std::vector<glm::vec2> outline = {{x0, 0.0f}, {x1, 0.0f}};
			if(b1 < x1){
				outline.insert(outline.end(), {{x1, notchZ}, {b1, notchZ}});
			}
			outline.insert(outline.end(), {{b1, -depth}, {b0, -depth}});
			if(b0 > x0){
				outline.insert(outline.end(), {{b0, notchZ}, {x0, notchZ}});
			}
			addBlock(mesh, outline, std::vector<float>(outline.size(), 0.0f), 0.08f * slot, 0.0f, whiteTop, {0.5f * (b0 + b1), 0.5f * notchZ}, float(note));
		}
		const glm::vec3 up(0.0f, 1.0f, 0.0f);
		addQuad(mesh, {{-aspect, bedY, 0.0f}, {aspect, bedY, 0.0f}, {aspect, bedY, -depth}, {-aspect, bedY, -depth}}, {up, up, up, up}, -1.0f);
		const std::vector<glm::vec2> slip = {{-aspect, gap}, {aspect, gap}, {aspect, slipFront}, {-aspect, slipFront}};
		addBlock(mesh, slip, std::vector<float>(4, 0.0f), 0.08f * slot, slipBottom, slipTop, {0.0f, 0.5f * (gap + slipFront)}, -1.0f);

		glBindBuffer(GL_ARRAY_BUFFER, _keys3DVertices);
		glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * mesh.size(), mesh.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		_keys3DVertexCount = GLsizei(mesh.size() / 7);
	}

	// Keys go down in 20 ms and come back up in 80 ms. A jump in time (seek, loop, first frame) sets them at once.
	const auto& actives = scene->getActiveKeys();
	const float dt = time - _keys3DTime;
	_keys3DTime = time;
	const bool jump = dt < 0.0f || dt > 0.25f;
	std::array<float, 128> press;
	std::array<int, 128> sets;
	for(int note = 0; note < 128; ++note){
		const bool down = actives[note] >= 0;
		if(down){
			_keys3DLastSet[note] = actives[note];
		}
		float& p = _keys3DPress[note];
		p = jump ? (down ? 1.0f : 0.0f) : glm::clamp(p + (down ? dt / 0.02f : -dt / 0.08f), 0.0f, 1.0f);
		press[note] = glm::smoothstep(0.0f, 1.0f, p);
		sets[note] = p > 0.0f ? _keys3DLastSet[note] : -1;
	}

	// Light from above, behind and to the left, so black keys cast toward the player; the shadow map covers the keyboard's box as the light sees it.
	const glm::vec3 lightDir = glm::normalize(glm::vec3(-0.4f, 1.0f, -0.7f));
	const glm::mat4 lightView = glm::lookAt(lightDir, glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
	glm::vec3 lo(1e9f);
	glm::vec3 hi(-1e9f);
	for(int i = 0; i < 8; ++i){
		const glm::vec4 corner((i & 1) ? aspect : -aspect, (i & 2) ? blackTop : slipBottom, (i & 4) ? slipFront : pivotZ, 1.0f);
		const glm::vec3 p(lightView * corner);
		lo = glm::min(lo, p);
		hi = glm::max(hi, p);
	}
	const glm::mat4 lightMvp = glm::ortho(lo.x, hi.x, lo.y, hi.y, -hi.z, -lo.z) * lightView;
	// Shadow lookups start this far off the surface, past the soft edge's reach, so a lit face does not shadow itself.
	const float shadowOffset = 4.0f * (hi.x - lo.x) / float(keysShadowSize.x);

	_programKeys3D.use();
	_programKeys3D.uniform("pivotZ", pivotZ);
	_programKeys3D.uniform("pressAngle", pressAngle);
	_programKeys3D.uniforms("press", press.size(), press.data());
	_programKeys3D.uniforms("sets", sets.size(), sets.data());
	_programKeys3D.uniform("lightMvp", lightMvp);
	_programKeys3D.uniform("shadowOffset", shadowOffset);
	_programKeys3D.uniform("lightDir", lightDir);
	_programKeys3D.uniform("eye", eye);
	_programKeys3D.uniform("whiteColor", whiteColor);
	_programKeys3D.uniform("blackColor", blackColor);
	_programKeys3D.uniforms("majorColor", majorColors.size(), majorColors.data());
	_programKeys3D.uniforms("minorColor", minorColors.size(), minorColors.data());
	_programKeys3D.uniform("highlightKeys", highlightKeys);

	GLint framebuffer = 0;
	GLint viewport[4];
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &framebuffer);
	glGetIntegerv(GL_VIEWPORT, viewport);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glBindVertexArray(_vaoKeys3D);

	// Depth seen from the light. The map must not be bound for reading while it is the target.
	_programKeys3D.texture("shadowMap", 0, GL_TEXTURE_2D);
	_programKeys3D.uniform("shadowPass", true);
	_programKeys3D.uniform("mvp", lightMvp);
	glBindFramebuffer(GL_FRAMEBUFFER, _keysShadowFramebuffer);
	glViewport(0, 0, keysShadowSize.x, keysShadowSize.y);
	glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(2.0f, 4.0f);
	glDrawArrays(GL_TRIANGLES, 0, _keys3DVertexCount);
	glDisable(GL_POLYGON_OFFSET_FILL);

	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
	glClear(GL_DEPTH_BUFFER_BIT);
	_programKeys3D.texture("shadowMap", _keysShadowMap, GL_TEXTURE_2D);
	_programKeys3D.uniform("shadowPass", false);
	_programKeys3D.uniform("mvp", mvp);
	glDrawArrays(GL_TRIANGLES, 0, _keys3DVertexCount);

	glBindVertexArray(0);
	glEnable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glUseProgram(0);
}

void Renderer::drawPedals(const std::shared_ptr<MIDIScene>& scene, float time, const glm::vec2 & invScreenSize, const State::PedalsState & state, float keyboardHeight, bool horizontalMode) {

	glEnable(GL_BLEND);
	_programPedals.use();

	// Adjust for aspect ratio.
	const float rat = invScreenSize.y/invScreenSize.x;
	const glm::vec2 ratioScale = (rat < 1.0f) ? glm::vec2(1.0f, rat) : glm::vec2(1.0f/rat, 1.0f);
	const glm::vec2 globalScale = 2.0f /* quad is -0.5,0.5 */ * state.size * ratioScale * glm::vec2(1.2f, 1.0f)  /* existing pedal ratio correction */;
	const State::PedalsState::Location mode = state.location;
	const float vertSign  = (mode == State::PedalsState::TOPLEFT || mode == State::PedalsState::TOPRIGHT) ? 1.0f : -1.0f;
	const float horizSign = (mode == State::PedalsState::TOPLEFT || mode == State::PedalsState::BOTTOMLEFT) ? -1.0f : 1.0f;
	const float safetyMargin = 0.02f;
	glm::vec2 globalShift = glm::vec2(horizSign, vertSign) * (1.0f - 0.5f * globalScale - safetyMargin * ratioScale);

	// If at the bottom, shift above the keyboard.
	if(horizontalMode){
		if(mode == State::PedalsState::TOPLEFT || mode == State::PedalsState::BOTTOMLEFT){
			globalShift[0] += 2.0f * keyboardHeight;
		}
	} else {
		if(mode == State::PedalsState::BOTTOMLEFT || mode == State::PedalsState::BOTTOMRIGHT){
			globalShift[1] += 2.0f * keyboardHeight;
		}
	}

	const float tightenShiftX = state.margin.x;
	const float tightenShiftY = state.margin.y;
	const GLuint textures[4] = { state.texSides[0], state.texCenter, state.texSides[1], state.texTop };


	// (1)soft, (2)sostenuto, (3)damper, (4)expression
	//   *-----------------^-----*
	//   |          (4)    | 20% |
	//   |-----------------v-----|
	//   |  (1)  |  (2)  |  (3)  |
	//   |       |   x   |       |
	//   |       |       |       |
	//   |<-35%->|<-30%->|<-35%->|
	//   |       |       |       |
	//   *-----------------------*

	const float expressionHeight = 0.20f;
	const float expressionWidth = 1.0f;
	const float sidesWidth = 0.35f;
	const float sidesHeight = 1.0f - expressionHeight;
	const float centralWidth = 1.0f - 2.0f * sidesWidth;
	const float centralHeight = sidesHeight;
	const float expressionShiftX = 0.0f;
	const float expressionShiftY = 0.5f * (1.0f - expressionHeight ) - tightenShiftY;
	const float sidesShiftX = 0.5f * (sidesWidth + centralWidth) - tightenShiftX;
	const float sidesShiftY = 0.5f * (sidesHeight-1.0f);
	const float centralShiftX = 0.0f;
	const float centralShiftY = 0.5f * (sidesHeight-1.0f);

	const glm::vec2 localScales[4] = {
		glm::vec2(sidesWidth, sidesHeight), glm::vec2(centralWidth, 	centralHeight),
		glm::vec2(sidesWidth, sidesHeight), glm::vec2(expressionWidth,	expressionHeight)
	};
	// By default all quads are centered at 0.5,0.5 in local space, we need to shift them post-scaling.
	const glm::vec2 localShifts[4] = {
		glm::vec2(-sidesShiftX, sidesShiftY), glm::vec2(centralShiftX,		centralShiftY),
		glm::vec2( sidesShiftX, sidesShiftY), glm::vec2(expressionShiftX,	expressionShiftY)
	};

	const MIDIScene::Pedals& pedals = scene->getPedals();
	const float actives[4] = { pedals.soft, pedals.sostenuto, pedals.damper, pedals.expression };
	const glm::vec3* colors[4] = { &state.leftColor, &state.centerColor, &state.rightColor, &state.topColor};
	// Uniforms setup.
	_programPedals.uniform("pedalOpacity", state.opacity);

	glBindVertexArray(_vaoQuadWithNoteData);
	if(state.merge){
		const float active = (glm::max)((glm::max)(actives[0], actives[1]), (glm::max)(actives[2], actives[3]));
		// We want to move the central pedal to the side
		const glm::vec2 localShift = glm::vec2(horizSign, vertSign) * glm::vec2(sidesShiftX, 0.5f * expressionHeight);

		_programPedals.uniform("scale", globalScale * localScales[1]);
		_programPedals.uniform("shift", globalShift + globalScale * localShift);
		_programPedals.uniform("pedalFlag", active);
		_programPedals.uniform("mirror", false);
		_programPedals.texture("pedalTexture", textures[1], GL_TEXTURE_2D);
		_programPedals.uniform("pedalColor", *colors[1]);

		glDrawElements(GL_TRIANGLES, int(_vaoQuadWithKeyData), GL_UNSIGNED_INT, (void*)0);
	} else {
		for(unsigned int i = 0; i < 4; ++i){
			_programPedals.uniform("scale", globalScale * localScales[i]);
			_programPedals.uniform("shift", globalShift + globalScale * localShifts[i]);
			_programPedals.uniform("pedalFlag", actives[i]);
			_programPedals.texture("pedalTexture", textures[i], GL_TEXTURE_2D);
			_programPedals.uniform("mirror", state.mirror && (i==2));
			_programPedals.uniform("pedalColor", *colors[i]);
			glDrawElements(GL_TRIANGLES, int(_vaoQuadWithKeyData), GL_UNSIGNED_INT, (void*)0);
		}
	}

	glBindVertexArray(0);
	glUseProgram(0);
	glDisable(GL_BLEND);
}

void Renderer::drawWaves(const std::shared_ptr<MIDIScene>& scene, float time, const glm::vec2 & invScreenSize, const State::WaveState & state, float keyboardHeight) {

	_programWave.use();
	glEnable(GL_BLEND);
	glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ONE);

	// Uniforms setup.
	_programWave.uniform("waveColor", state.color);
	_programWave.uniform("keyboardSize", keyboardHeight);
	_programWave.uniform("waveOpacity", state.opacity);
	_programWave.uniform("spread", state.spread);
	glBindVertexArray(_vaoWave);

	// Fixed initial parameters.
	const float ampls[4] = {-0.023f, -0.011f, 0.017f, 0.009f};
	const float freqs[4] = {10.3f, -8.27f, -4.4f, 13.7f};
	const float phases[4] = {5.2f, 4.7f, 9.3f, -7.1f};
	// Render multiple waves with additive blending.
	for(int i = 0; i < 4; ++i){
		const float ampl = state.amplitude * ampls[i];
		const float freq = state.frequency * freqs[i];
		const float phase = phases[i] * time * state.speed + float(i+1) * 7.39f;
		_programWave.uniform("amplitude", ampl);
		_programWave.uniform("freq", freq);
		_programWave.uniform("phase", phase);
		glDrawElements(GL_TRIANGLES, int(_wavePrimitiveCount), GL_UNSIGNED_INT, (void*)0);
	}

	glBindVertexArray(0);
	glUseProgram(0);

	_programWaveNoise.use();
	_programWaveNoise.uniform("keyboardSize", keyboardHeight);
	_programWaveNoise.uniform("scale", state.noiseSize * 0.5f);

	_programWaveNoise.texture("textureNoise", _texNoise, GL_TEXTURE_2D);
	_programWaveNoise.uniform("offset", time * state.speed * 0.05f);
	_programWaveNoise.uniform("waveColor", state.color);
	_programWaveNoise.uniform("noiseScale", state.frequency);
	_programWaveNoise.uniform("waveOpacity", state.noiseIntensity);
	_programWaveNoise.uniform("inverseScreenSize", invScreenSize);

	glBindVertexArray(_vaoQuadWithKeyData);
	glDrawElements(GL_TRIANGLES, int(_vaoQuadWithKeyData), GL_UNSIGNED_INT, (void*)0);

	glBindVertexArray(0);
	glUseProgram(0);
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
	glDisable(GL_BLEND);
}

void Renderer::drawScore(const std::shared_ptr<MIDIScene>& scene, float time, const glm::vec2 & invScreenSize, const State::ScoreState & state, float measureScale, float qualityScale, float keyboardHeight, bool horizontalMode, bool reverseScroll){

	const glm::vec2 pixelSize = qualityScale * (horizontalMode ? glm::vec2(invScreenSize.y, invScreenSize.x) : invScreenSize);

	// Draw vertical lines.
	if(state.vLines){
		// 7 major keys.
		const int firstBar = (_minKeyMajor + 6) / 7;
		const int barCount = _keyCount / 7 + 1;

		// minKeyMajor maps to -1
		// _minKeyMajor + _keyCount maps to 1
		float firstBarCoord = (firstBar * 7 - _minKeyMajor) / (float)_keyCount;
		firstBarCoord = 2.0f * firstBarCoord - 1.0f;
		float nextBarDeltaCoord = 7.0f / (float)_keyCount;
		nextBarDeltaCoord *= 2.0f;

		_programScoreBars.use();
		_programScoreBars.uniform("baseOffset", glm::vec2(firstBarCoord, 0.0f));
		_programScoreBars.uniform("nextOffset", glm::vec2(nextBarDeltaCoord, 0.0f));
		_programScoreBars.uniform("scale", glm::vec2(state.vLinesWidth * pixelSize.x, 1.0f));
		_programScoreBars.uniform("color", state.vLinesColor);
		glBindVertexArray(_vaoQuad);
		glDrawElementsInstanced(GL_TRIANGLES, int(_quadPrimitiveCount), GL_UNSIGNED_INT, (void*)0, barCount);
		glBindVertexArray(0);
		glUseProgram(0);
	}

	if(state.hLines || state.digits){

		// Bar 0 starts at the scene's grid origin.
		const float gridTime = time - float(scene->gridOrigin());
		const float currentAbscisse = gridTime/float(scene->secondsPerMeasure());

		// Find the measure at the bottom of the screen if forward, top of the screen if bottom
		// TODO: (MV) explain the magic values.
		const float magigValue0 = 2.f; // Maybe UV -> NDC ?
		const float magicValue1 = measureScale * float(scene->secondsPerMeasure()); // Units don't make sense...

		const float distanceToScreenEdge = magigValue0 * (reverseScroll ? (1.0f - keyboardHeight) : (keyboardHeight) );

		float firstMesureAbscisse = currentAbscisse - distanceToScreenEdge / measureScale;
		int firstMeasure = int(std::floor(firstMesureAbscisse));
		const float firstMeasureTime = float(scene->secondsPerMeasure()) * float(firstMeasure);

		const float direction = (reverseScroll ? -1.0f : 1.0f);
		const float keyboardPos = 2.0f * keyboardHeight - 1.0f;

		const float nextBarDeltaCoord = direction * magicValue1;

		float firstBarCoord = direction * (firstMeasureTime - gridTime) * measureScale + keyboardPos;
		
		// Corrective step.
		// while the next bar is not on screen, keep stepping forwards.
		while( direction * ( firstBarCoord + nextBarDeltaCoord ) < -1.f )
		{
			firstBarCoord += nextBarDeltaCoord;
			++firstMeasure;
		}
		// while the first bar is on screen, keep stepping backwards.
		while( direction * ( firstBarCoord ) > -1.f )
		{
			firstBarCoord -= nextBarDeltaCoord;
			--firstMeasure;
		}

		const int barCount = std::ceil( std::abs( 2.f / nextBarDeltaCoord ) ) + 2; // Two extra bars because of corrective stepping.

		// Draw horizontal lines
		if(state.hLines){
			_programScoreBars.use();
			_programScoreBars.uniform("baseOffset", glm::vec2(0.0f, firstBarCoord));
			_programScoreBars.uniform("nextOffset", glm::vec2(0.0f, nextBarDeltaCoord));
			_programScoreBars.uniform("scale", glm::vec2(1.0f, state.hLinesWidth * pixelSize.y));
			_programScoreBars.uniform("color", state.hLinesColor);
			glBindVertexArray(_vaoQuad);
			glDrawElementsInstanced(GL_TRIANGLES, int(_quadPrimitiveCount), GL_UNSIGNED_INT, (void*)0, barCount);
			glBindVertexArray(0);
			glUseProgram(0);
		}

		if(state.digits){
			const float textScale = state.digitsScale;
			// Based on texture size.
			const glm::vec2 digitResolution = glm::vec2(200.0f, 256.0f);
			const glm::vec2 digitSize = textScale * qualityScale * invScreenSize * digitResolution;
			// Ensure at least one measure.
			const float maxMeasureCount = float(scene->duration() / scene->secondsPerMeasure()) + 1;
			// Add one extra digit so that postroll measures are displayed entirely in all cases.
			const float digitCount = (std::floor(std::log10((std::max)(1.f, maxMeasureCount))) + 1) + 1;

			const glm::vec2 offset = 2.0f * digitSize * state.digitsOffset;
			const glm::vec2 margin = horizontalMode ? glm::vec2(offset.y, offset.x) : offset;

			_programScoreLabels.use();
			_programScoreLabels.uniform("baseOffset", glm::vec2(-1.0f, firstBarCoord) + margin);
			_programScoreLabels.uniform("nextOffset", glm::vec2(0.0f, nextBarDeltaCoord));
			_programScoreLabels.uniform("scale", digitSize);
			_programScoreLabels.uniform("color", state.digitsColor);
			_programScoreLabels.uniform("maxDigitCount", int(digitCount));
			_programScoreLabels.uniform("firstMeasure", firstMeasure);
			_programScoreLabels.texture("font", _texFont, GL_TEXTURE_2D);
			glBindVertexArray(_vaoQuad);
			glDrawElementsInstanced(GL_TRIANGLES, int(_quadPrimitiveCount), GL_UNSIGNED_INT, (void*)0, barCount);
			glBindVertexArray(0);
			glUseProgram(0);
		}
	}
}

void Renderer::clean(){
	GLuint vaos[] = {_vaoQuad, _vaoQuadWithNoteData, _vaoQuadWithKeyData, _vaoWave, _vaoKeys3D};
	GLuint buffers[] = {_quadVertices, _quadIndices, _waveVertices, _waveIndices, _notesDataBuffer, _keysDataBuffer, _keysGainBuffer, _keys3DVertices };
	glDeleteVertexArrays(sizeof(vaos)/sizeof(vaos[0]), vaos);
	glDeleteBuffers(sizeof(buffers)/sizeof(buffers[0]), buffers);
	glDeleteFramebuffers(1, &_keysShadowFramebuffer);
	glDeleteTextures(1, &_keysShadowMap);

	_programNotes.clean();
	_programFlashes.clean();
	_programParticules.clean();
	_programKeyMinors.clean();
	_programKeyMajors.clean();
	_programKeys3D.clean();
	_programPedals.clean();
	_programWave.clean();
	_programWaveNoise.clean();
	_programScoreBars.clean();
	_programScoreLabels.clean();
}
