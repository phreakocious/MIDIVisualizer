#version 330

in INTERFACE {
	vec3 pos;
	vec3 local; // Position before the press turns the key.
	vec3 n;
	vec4 lightPos;
	flat int material; // 0 white key, 1 black key, 2 case.
	flat int set;
	flat float press;
} In ;

#define SETS_COUNT 12

uniform bool shadowPass; // Depth only, seen from the light.
uniform sampler2DShadow shadowMap;
uniform vec3 eye;
uniform float whiteTop; // Height of the white keys' tops at rest.
uniform float gap; // Width of the slots between keys.
uniform float lip; // How far a white key's cap overhangs its front.
uniform float lipBottom; // Height of the cap's underside.
uniform vec3 lightDir;
uniform vec3 whiteColor;
uniform vec3 blackColor;
uniform vec3 majorColor[SETS_COUNT];
uniform vec3 minorColor[SETS_COUNT];
uniform bool highlightKeys;

out vec4 fragColor;

// Share of the light that reaches this point: a 4x4 grid of lookups, for a soft shadow edge.
float lit(){
	vec3 p = In.lightPos.xyz / In.lightPos.w * 0.5 + 0.5;
	vec2 texel = 1.0 / vec2(textureSize(shadowMap, 0));
	float sum = 0.0;
	for(int y = 0; y < 4; ++y){
		for(int x = 0; x < 4; ++x){
			sum += texture(shadowMap, vec3(p.xy + (vec2(x, y) - 1.5) * 1.5 * texel, p.z));
		}
	}
	return sum / 16.0;
}

void main(){
	if(shadowPass){
		fragColor = vec4(1.0);
		return;
	}
	bool black = In.material == 1;
	vec3 base = In.material == 0 ? whiteColor : blackColor;
	if(highlightKeys && In.set >= 0){
		base = mix(base, black ? minorColor[In.set] : majorColor[In.set], In.press);
	}
	// Pure black would show no shape.
	base = max(base, vec3(0.03));
	vec3 n = normalize(In.n);
	vec3 view = normalize(eye - In.pos);
	float shadow = lit();
	// Walls in the slots between keys see little light, and the slots are too narrow for the shadow map:
	// darken them by depth below the white tops. White key fronts face open air above the key slip.
	float occlusion = 1.0;
	if(In.material != 2 && n.y < 0.7 && !(In.material == 0 && n.z > 0.5)){
		occlusion = 1.0 / (1.0 + 2.0 * max(whiteTop - In.pos.y, 0.0) / gap);
	}
	// Just under the cap's lip, a white key's front sees less of the room: a thin dark line, as on a real key.
	if(In.material == 0 && n.z > 0.5 && In.local.z < 0.5 * lip){
		occlusion = mix(0.5, 1.0, smoothstep(0.0, 3.0 * lip, lipBottom - In.local.y));
	}
	// Scaled so a lit top face shows the key's own color.
	float diffuse = max(dot(n, lightDir), 0.0) / lightDir.y * shadow;
	// Black keys are glossy, the case less so, white keys barely.
	float gloss = black ? 0.6 : (In.material == 2 ? 0.3 : 0.15);
	float spec = pow(max(dot(n, normalize(lightDir + view)), 0.0), black ? 80.0 : 40.0) * 0.6 * gloss * shadow;
	// Glossy keys mirror a soft room, bright above and dark below, so tops and sides stay apart anywhere on screen;
	// a single light's highlight only reaches the camera near the middle. A rounded edge's blended normal
	// can face away from the eye: no reflection there, or the thin far-side bevels sparkle.
	float facing = dot(n, view);
	float room = mix(0.05, 1.0, smoothstep(-0.2, 0.9, reflect(-view, n).y));
	float fresnel = 0.04 + 0.96 * pow(1.0 - max(facing, 0.0), 5.0);
	float reflection = facing > 0.0 ? room * gloss * mix(0.2, 1.0, fresnel) : 0.0;
	fragColor = vec4((base * min(0.45 + 0.55 * diffuse, 1.0) + vec3(spec + reflection)) * occlusion, 1.0);
}
