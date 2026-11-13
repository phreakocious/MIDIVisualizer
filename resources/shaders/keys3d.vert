#version 330

layout(location = 0) in vec3 v; // Unit box corner, in [-0.5, 0.5].
layout(location = 1) in vec3 n; // Face normal.
layout(location = 2) in vec4 box; // x center, width, length, height.
layout(location = 3) in vec2 key; // Pressed set (-1 at rest), 1 for a black key.

uniform mat4 mvp;
uniform float backZ; // Back edge of the keyboard, where keys pivot.
uniform float pressAngle; // Radians a pressed key tips down.

out INTERFACE {
	vec3 pos;
	vec3 n;
	flat vec2 key;
} Out ;

void main(){
	// At rest a key sits on the floor, back edge on the pivot line, front toward the camera.
	vec3 p = vec3(box.x + v.x * box.y, (v.y + 0.5) * box.w, backZ + (v.z + 0.5) * box.z);
	// A pressed key tips about its back edge: the front goes down.
	float a = key.x >= 0.0 ? pressAngle : 0.0;
	float c = cos(a);
	float s = sin(a);
	float dz = p.z - backZ;
	p = vec3(p.x, c * p.y - s * dz, backZ + s * p.y + c * dz);
	Out.n = vec3(n.x, c * n.y - s * n.z, s * n.y + c * n.z);
	Out.pos = p;
	Out.key = key;
	gl_Position = mvp * vec4(p, 1.0);
}
