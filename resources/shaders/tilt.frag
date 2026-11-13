#version 330

in INTERFACE {
	vec2 uv;
} In ;

uniform sampler2D screenTexture;
uniform vec4 fogColor;

out vec4 fragColor;

void main(){
	// Fade the far end into the background so notes do not stop at a hard edge.
	fragColor = mix(texture(screenTexture, In.uv), fogColor, smoothstep(0.6, 1.0, In.uv.y));
}
