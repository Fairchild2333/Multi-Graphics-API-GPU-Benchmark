// GPU Burn ES 2.0 legacy v1 - a fixed-count quasicrystal plasma for OpenGL ES 2.0.
//
// Its own score group (gpu_burn_es2_legacy_v1): the GPU Burn of the other APIs
// needs unsigned integers, bit operations and a loop bound taken from a
// uniform, none of which GLSL ES 1.00 has.
//
// The picture is the sum of travelling waves in seven directions, a seventh of
// a turn apart, which gives a pattern with the symmetry of a kaleidoscope.
// Every step adds the next wave and lets it bend the plane a little for the
// waves that follow, so no step can be worked out without the ones before it.
//
// - Written for mediump: a Tegra 3 has nothing better in a fragment shader.
//   Coordinates stay within a few units and the arguments of sin and cos
//   within a few tens, so ten bits of precision keep the picture whole. The
//   direction is turned and brought back to unit length in every step rather
//   than computed from the loop counter, which mediump cannot count far.
// - BURN_STEPS is defined by the engine in front of this text. GLSL ES 1.00
//   wants a constant loop bound, and the score contract wants exactly that
//   many steps per pixel: no break, no discard.
// - A frame is two draws that are added together (the engine blends ONE, ONE),
//   so a GPU that removes hidden surfaces cannot skip the first one.

precision mediump float;

uniform float uPhase;   // animation phase, 0 .. 2*pi, wrapped by the engine
uniform float uPass;    // 0.0 or 1.0: the draw within the frame

varying vec2 vUV;

const float kTurnCos = 0.6234898;   // cos(2 pi / 7)
const float kTurnSin = 0.7818315;   // sin(2 pi / 7)

void main() {
    vec2 p = vUV * 2.0 - 1.0;
    p.x *= 16.0 / 9.0;
    // The second draw shows the pattern mirrored and a little larger.
    p = mix(p, p.yx * 1.12, uPass);

    vec2 dir = vec2(cos(uPhase), sin(uPhase));
    float bend = 0.30 / float(BURN_STEPS);
    float field = 0.0;
    float ridge = 0.0;

    for (int i = 0; i < BURN_STEPS; ++i) {
        // The next of the seven directions, kept at unit length.
        dir = vec2(kTurnCos * dir.x - kTurnSin * dir.y,
                   kTurnSin * dir.x + kTurnCos * dir.y);
        dir *= inversesqrt(dot(dir, dir));

        float wave = sin(dot(p, dir) * 9.0 + uPhase);
        float cross = cos(dot(p, dir.yx) * 5.0 - uPhase);
        field += wave;
        ridge += wave * cross;

        // This wave bends the plane for the ones that follow.
        p += dir.yx * (wave * bend);
    }

    float perStep = 1.0 / float(BURN_STEPS);
    field *= perStep;
    ridge *= perStep;

    vec3 colour = vec3(0.5 + 0.5 * sin(field * 9.0 + uPhase),
                       0.5 + 0.5 * sin(field * 6.0 + ridge * 5.0 + 2.1),
                       0.5 + 0.5 * cos(ridge * 9.0 - uPhase));
    vec2 centre = vUV - 0.5;
    float vignette = 1.0 - 1.4 * dot(centre, centre);

    // Half the picture per draw; the two draws add up to one frame.
    gl_FragColor = vec4(colour * vignette * vec3(0.46, 0.34, 0.52), 1.0);
}
