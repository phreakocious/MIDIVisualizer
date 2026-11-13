#version 330

layout(location = 0) in vec3 v;
layout(location = 1) in vec3 n;
layout(location = 2) in float note; // MIDI note of the key, -1 for the case.

uniform mat4 mvp;
uniform mat4 lightMvp;
uniform float shadowOffset; // Shadow lookups start this far out along the normal.
uniform float pivotZ; // Keys turn about the line y = 0, z = pivotZ.
uniform float pressAngle; // Radians a key turns when fully down.
uniform float press[128]; // 0 at rest, 1 fully down.
uniform int sets[128]; // Color set shown on each key, -1 for none.

out INTERFACE {
	vec3 pos;
	vec3 n;
	vec4 lightPos;
	flat int material; // 0 white key, 1 black key, 2 case.
	flat int set;
	flat float press;
} Out ;

void main(){
	int k = int(note);
	bool isKey = k >= 0;
	float a = isKey ? press[k] * pressAngle : 0.0;
	float c = cos(a);
	float s = sin(a);
	// Turn about the pivot line: the front goes down.
	float dz = v.z - pivotZ;
	vec3 p = vec3(v.x, c * v.y - s * dz, pivotZ + s * v.y + c * dz);
	Out.n = vec3(n.x, c * n.y - s * n.z, s * n.y + c * n.z);
	Out.pos = p;
	Out.lightPos = lightMvp * vec4(p + shadowOffset * Out.n, 1.0);
	// Black keys are 1, 3, 6, 8 and 10 in each octave: bits of 0x54A.
	Out.material = isKey ? ((0x54A >> (k % 12)) & 1) : 2;
	Out.set = isKey ? sets[k] : -1;
	Out.press = isKey ? press[k] : 0.0;
	gl_Position = mvp * vec4(p, 1.0);
}
