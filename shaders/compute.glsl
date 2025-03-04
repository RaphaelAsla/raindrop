#version 450
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

struct Raindrop {
    vec2 position;
    vec2 velocity;
};
layout(std430, binding = 0) buffer Raindrops {
    Raindrop drops[];
};

struct Ball {
    vec2 position;
    float rotation;
    float padding;
};
layout(std430, binding = 1) buffer BallData {
    Ball ball;
};

layout(rgba32f, binding = 0) uniform image2D outputTexture;

uniform float timeElapsed;
uniform float width;
uniform float height;
const bool FILL_UP = true;
const float DROP_WIDTH = 1.25;
const float DROP_LENGTH = 8.0;
const float BALL_RADIUS = 15.0;
const float ROTATION_SPEED = 0.3;
const int ARC_SEGMENTS = 32;

// Wave calculation
float getWaveY(float x) {
    float yOffset = height - (timeElapsed * 0.5);
    float u = (x * 0.05) + timeElapsed;
    float v = (x * -0.025) + timeElapsed;
    return yOffset + 10.0 * sin(u) * cos(v);
}

// Point-in-polygon test (simplified for convex shape)
bool pointInPolygon(vec2 p, vec2 vertices[35], int numVertices) {
    int winding = 0;
    for (int i = 0; i < numVertices; i++) {
        vec2 v0 = vertices[i];
        vec2 v1 = vertices[(i + 1) % numVertices];
        if (v0.y <= p.y) {
            if (v1.y > p.y && (v1.x - v0.x) * (p.y - v0.y) - (p.x - v0.x) * (v1.y - v0.y) > 0)
                winding++;
        } else {
            if (v1.y <= p.y && (v1.x - v0.x) * (p.y - v0.y) - (p.x - v0.x) * (v1.y - v0.y) < 0)
                winding--;
        }
    }
    return winding != 0;
}

void main() {
    uint id = gl_GlobalInvocationID.x;

    // Update Raindrops
    if (id < 500) { // NUM_DROPS = 500
        Raindrop drop = drops[id];
        drop.position.x += drop.velocity.x;
        drop.position.y += drop.velocity.y;

        float tailLength = abs(drop.velocity.x) + abs(drop.velocity.y);
        if (drop.position.x + tailLength * DROP_LENGTH / 2.0 < 0) {
            drop.position.x = width;
        } else if (drop.position.x - tailLength * DROP_LENGTH / 2.0 > width) {
            drop.position.x = 0;
        }

        if (FILL_UP) {
            float waveY = getWaveY(drop.position.x);
            if (drop.position.y > waveY) {
                drop.position.y = 0;
            }
        } else if (drop.position.y - tailLength * DROP_LENGTH / 2.0 > height) {
            drop.position.y = 0;
        }
        drops[id] = drop;

        // Draw Raindrop
        float theta = atan(drop.velocity.y, drop.velocity.x) - 3.14159 / 2.0;
        float baseRadius = DROP_WIDTH * 2.0;
        float triHeight = tailLength * DROP_LENGTH / 2.0;
        vec2 center = drop.position;

        // Define raindrop vertices
        vec2 vertices[35]; // ARC_SEGMENTS + 3
        vertices[0] = vec2(0, 0); // Center
        for (int i = 0; i <= ARC_SEGMENTS; i++) {
            float angle = float(i) / ARC_SEGMENTS * 3.14159;
            float vxArc = baseRadius * cos(angle);
            float vyArc = baseRadius * sin(angle);
            vertices[i + 1] = vec2(vxArc, vyArc);
        }
        vertices[33] = vec2(-baseRadius, 0);
        vertices[34] = vec2(0, -triHeight);

        // Rotate and translate vertices
        mat2 rot = mat2(cos(theta), -sin(theta), sin(theta), cos(theta));
        for (int i = 0; i < 35; i++) {
            vertices[i] = rot * vertices[i] + center;
        }

        // Bounding box
        vec2 minBB = center - vec2(baseRadius + triHeight);
        vec2 maxBB = center + vec2(baseRadius + triHeight);
        minBB = max(minBB, vec2(0));
        maxBB = min(maxBB, vec2(width, height));

        // Rasterize
        for (int px = int(minBB.x); px <= int(maxBB.x); px++) {
            for (int py = int(minBB.y); py <= int(maxBB.y); py++) {
                vec2 pixel = vec2(px + 0.5, py + 0.5);
                if (pointInPolygon(pixel, vertices, 35)) {
                    imageStore(outputTexture, ivec2(px, py), vec4(0.4, 0.8, 1.0, 0.5));
                }
            }
        }
    }

    // Update and Draw Ball (handle as a single invocation)
    if (id == 500) {
        float yOffset = height - (timeElapsed * 0.001 * 500.0);
        float u = (ball.position.x * 0.05) + timeElapsed;
        float v = (ball.position.x * -0.025) + timeElapsed;
        float waveY = yOffset + 10.0 * sin(u) * cos(v);
        float slope = 10.0 * (0.05 * cos(u) * cos(v) + 0.025 * sin(u) * sin(v));
        float accel = -0.03125 * sin(u) * cos(v);

        ball.position.x -= (accel < 0) ? 1.1 : 0.9;
        if (ball.position.x < 0) ball.position.x += width;
        ball.position.y = waveY - BALL_RADIUS * 0.6;
        ball.rotation += ROTATION_SPEED * slope;
        
        // Draw Ball (6 segments + center)
        vec3 colors[6] = vec3[](
            vec3(1.0, 0.0, 0.0), vec3(0.0, 0.0, 1.0), vec3(1.0, 1.0, 0.0),
            vec3(0.0, 1.0, 0.0), vec3(1.0, 1.0, 1.0), vec3(1.0, 0.5, 0.0)
        );
        vec2 center = ball.position;
        float angleStep = 2.0 * 3.14159 / 6.0;
        mat2 rot = mat2(cos(ball.rotation), -sin(ball.rotation), sin(ball.rotation), cos(ball.rotation));

        // Bounding box for ball
        vec2 minBB = center - vec2(BALL_RADIUS);
        vec2 maxBB = center + vec2(BALL_RADIUS);
        minBB = max(minBB, vec2(0));
        maxBB = min(maxBB, vec2(width, height));

        for (int px = int(minBB.x); px <= int(maxBB.x); px++) {
            for (int py = int(minBB.y); py <= int(maxBB.y); py++) {
                vec2 relPos = vec2(px + 0.5, py + 0.5) - center;
                float dist = length(relPos);
                if (dist <= BALL_RADIUS) {
                    relPos = rot * relPos;
                    float angle = atan(relPos.y, relPos.x);
                    if (angle < 0) angle += 2.0 * 3.14159;
                    int seg = int(angle / angleStep) % 6;
                    vec4 color = (dist <= BALL_RADIUS * 0.2) ? vec4(1.0) : vec4(colors[seg], 1.0);
                    imageStore(outputTexture, ivec2(px, py), color);
                }
            }
        }
    }

    // Draw Water (single invocation)
    if (id == 501) {
        for (int x = 0; x < int(width); x++) {
            float waveY = getWaveY(float(x));
            for (int y = int(waveY); y < int(height); y++) {
                imageStore(outputTexture, ivec2(x, y), vec4(0.4, 0.8, 1.0, 0.5));
            }
        }
    }
}
