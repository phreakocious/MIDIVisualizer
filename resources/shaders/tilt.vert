#version 330

layout(location = 0) in vec3 v;

uniform mat4 mvp;
uniform float aspect;
uniform vec2 rangeV; // Rows of the flat frame this strip shows.
uniform vec2 rangeZ; // Where the strip lies on the floor.

out INTERFACE {
	vec2 uv;
} Out ;

void main(){
	float t = v.y * 0.5 + 0.5;
	Out.uv = vec2(v.x * 0.5 + 0.5, mix(rangeV.x, rangeV.y, t));
	vec3 p = vec3(v.x * aspect, 0.0, mix(rangeZ.x, rangeZ.y, t));
	gl_Position = mvp * vec4(p, 1.0);
}
