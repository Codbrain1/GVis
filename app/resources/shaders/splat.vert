#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in float aScalar;

uniform mat4  uMVP;
uniform float uPointScale;
uniform float uWorldRadius;
uniform bool  uShowOutRangeParticles;
uniform float uRangeMin;
uniform float uRangeMax;

out float vScalar;

void main() {
    bool inRange = aScalar >= uRangeMin && aScalar <= uRangeMax;
    if (!uShowOutRangeParticles && !inRange) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        gl_PointSize = 0.0;
    } else {
        gl_Position = uMVP * vec4(aPos, 1.0);
        gl_PointSize = 4.0 * uPointScale * uWorldRadius;
    }
    vScalar = aScalar;
}