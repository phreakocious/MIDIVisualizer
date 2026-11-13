#version 330

in INTERFACE {
	vec3 pos;
	vec3 n;
	flat vec2 key;
} In ;

#define SETS_COUNT 12

uniform vec3 eye;
uniform vec3 whiteColor;
uniform vec3 blackColor;
uniform vec3 majorColor[SETS_COUNT];
uniform vec3 minorColor[SETS_COUNT];
uniform bool highlightKeys;

out vec4 fragColor;

void main(){
	int set = int(In.key.x);
	bool black = In.key.y > 0.5;
	vec3 base = black ? blackColor : whiteColor;
	if(highlightKeys && set >= 0){
		base = black ? minorColor[set] : majorColor[set];
	}
	// One light above, in front and to the left; black keys are glossier.
	vec3 n = normalize(In.n);
	vec3 l = normalize(vec3(-0.3, 1.0, 0.5));
	vec3 h = normalize(l + normalize(eye - In.pos));
	float diffuse = max(dot(n, l), 0.0);
	float spec = pow(max(dot(n, h), 0.0), black ? 64.0 : 32.0) * (black ? 0.5 : 0.15);
	fragColor = vec4(base * (0.35 + 0.65 * diffuse) + vec3(spec), 1.0);
}
