#version 430 core
out vec4 FragColor;

uniform vec2 resolution;
uniform float time;
uniform int numDrops;

struct Raindrop {
	vec2 pos;
	vec2 vel;
};


layout (std430, binding = 0) buffer RaindropBuffer {
    Raindrop raindrops[];
};

struct Ball {
	vec2 pos;
	float radius;
	float rotation;
};

Ball ball = Ball(vec2(1.0, 0.0), 0.01, 2.0);

float wave(float x) {
    float wave_freq = 50.0;
    float wave_ampl = 0.03;
    float wave_speed = 1.5;
	return clamp(time * numDrops * 0.000025, 0.0, 1.0) + wave_ampl * sin((x * wave_freq) + time * wave_speed) * cos((x * -wave_freq * 0.5) + time * wave_speed);
}

void main() {
    vec2 uv = gl_FragCoord.xy / resolution;
    float aspect = resolution.x / resolution.y;

    uv.x *= aspect;

    vec3 color = vec3(0.0);

    for (int i = 0; i < numDrops; i++) {
        vec2 initialPos = raindrops[i].pos.xy * aspect;
		vec2 dropPos = initialPos + raindrops[i].vel * time;

        dropPos.x = mod(dropPos.x, aspect);  
        dropPos.y = mod(-dropPos.y, 1.0); 

        float tailLength = 0.05; 
        vec2 tailDir = vec2(-raindrops[i].vel.x, raindrops[i].vel.y);
        vec2 toUV = uv - dropPos; // Vector from drop to current fragment

        float proj_len = clamp(dot(toUV, normalize(tailDir)), 0.0, tailLength); // projected length
        vec2 closestPoint = dropPos + normalize(tailDir) * proj_len; // Closest point on the tail line
        float dist = length(uv - closestPoint); // Distance to the tail line
        float radius = 0.002;

        if (dist < radius) {
            float tailFade = smoothstep(tailLength, 0.0, proj_len); // 1 at head, 0 at tail end
            float widthFade = smoothstep(radius, 0.0, dist); // Fade based on distance from center
            float intensity = tailFade * widthFade;
            color += vec3(0.0, 0.5, 1.0) * intensity;
        }
    }
    float wave_y = wave(uv.x);

    if (uv.y < wave_y - 0.005) {
		color = vec3(0.0, 0.5, 1.0);
	} else if (uv.y < wave_y) {
		color = vec3(0.0, 0.3, 1.0);
	}

	ball.pos.x = mod(ball.pos.x + 0.1 * time, aspect);
    ball.pos.y = wave(ball.pos.x) + ball.radius; 

    if (length(uv - ball.pos) < ball.radius) {
        color = vec3(1.0, 0.5, 0.0);
	}

    FragColor = vec4(color, 0.0);
}
