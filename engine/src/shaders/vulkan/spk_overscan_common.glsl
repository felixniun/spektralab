#ifndef SPK_OVERSCAN_COMMON_GLSL
#define SPK_OVERSCAN_COMMON_GLSL

const uint P_CW = 0u, P_CH = 1u, P_PX = 2u, P_FW = 3u, P_FH = 4u;
const uint P_A00 = 5u, P_A01 = 6u, P_A10 = 7u, P_A11 = 8u, P_CS = 9u, P_CT = 10u, P_CU = 11u, P_CV = 12u;
const uint P_VERTICAL = 13u;
const uint P_GS0 = 14u, P_GT0 = 15u, P_GW = 16u, P_GH = 17u, P_CORNER = 18u, P_PENUMBRA = 19u;
const uint P_WOB_A = 20u, P_WOB_PH = 32u;
const uint P_FILM_W = 44u, P_FOG_AMP = 45u, P_FOG_WIDTH = 46u, P_FOG_SEED = 47u, P_FOG_PERIOD = 48u;
const uint P_FOG_R = 49u, P_FOG_G = 50u, P_FOG_B = 51u, P_N_LEAKS = 52u;
const uint P_PERFORATED = 83u, P_PERF_PITCH = 84u, P_PERF_W = 85u, P_PERF_H = 86u, P_PERF_EDGE = 87u, P_PERF_R = 88u, P_PERF_PHASE = 89u;
const uint P_HOLE_C = 90u, P_HOLE_M = 91u, P_HOLE_Y = 92u;
const uint P_FLARE_AMP = 93u, P_FLARE_WIDTH = 94u;
const uint P_GATE_R = 95u, P_N_Q = 99u, P_Q = 100u, K_Q_STRIDE = 10u;
const uint P_ROUGH_AMP = 180u, P_ROUGH_PERIOD = 181u, P_ROUGH_SEED = 182u;
const uint P_HOLES_LIGHT = 183u, P_LIGHT_R = 184u, P_LIGHT_G = 185u, P_LIGHT_B = 186u;
const uint P_PERF_SEED = 187u, P_LIGHT_FALL = 188u, P_LIGHT_DIR = 189u;
const uint P_WALL_T = 190u, P_WALL_VS = 191u, P_WALL_VT = 192u, P_WALL_PAR = 193u;
const uint P_BASE_R = 194u, P_BASE_G = 195u, P_BASE_B = 196u, P_WALL_GLOW = 197u, P_WALL_SCATTER = 198u;
const uint P_CARRIER = 199u, P_LEAKS = 200u, P_PAIR_ADV = 260u;
const float kPi = 3.14159265358979323846;

layout(set=0,binding=1,std430) readonly buffer Parameters { float p[]; };

vec2 canvas_to_film(float x, float y) {
    float u = (x + 0.5) * p[P_PX] - p[P_CU];
    float v = (y + 0.5) * p[P_PX] - p[P_CV];
    return vec2(p[P_A00] * u + p[P_A01] * v + p[P_CS], p[P_A10] * u + p[P_A11] * v + p[P_CT]);
}

uint hash_u(uint x) {
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    return x;
}
float hash_f(int i, uint seed) { return float(hash_u(uint(i) * 0x9E3779B9u ^ seed)) * (1.0 / 4294967295.0); }
float value_noise(float x, uint seed) {
    float i0 = floor(x), f = x - i0;
    float w = f * f * (3.0 - 2.0 * f);
    return mix(hash_f(int(i0), seed), hash_f(int(i0) + 1, seed), w);
}

float sd_quad(vec2 pos, uint q) {
    vec2 v[4] = vec2[4](vec2(p[q], p[q + 1u]), vec2(p[q + 2u], p[q + 3u]),
                        vec2(p[q + 4u], p[q + 5u]), vec2(p[q + 6u], p[q + 7u]));
    float d = dot(pos - v[0], pos - v[0]);
    float sgn = 1.0;
    for (uint i = 0u, j = 3u; i < 4u; j = i, ++i) {
        vec2 e = v[j] - v[i], w = pos - v[i];
        float ee = dot(e, e);
        if (ee < 1e-12) continue;
        vec2 b = w - e * clamp(dot(w, e) / ee, 0.0, 1.0);
        d = min(d, dot(b, b));
        bool c0 = pos.y >= v[i].y, c1 = pos.y < v[j].y, c2 = e.x * w.y > e.y * w.x;
        if ((c0 && c1 && c2) || (!c0 && !c1 && !c2)) sgn = -sgn;
    }
    return sgn * sqrt(d);
}

float gate_sdf(vec2 st) {
    float hw = 0.5 * p[P_GW], hh = 0.5 * p[P_GH];
    float ds = st.x - (p[P_GS0] + hw), dt = st.y - (p[P_GT0] + hh);
    float r = ds > 0.0 ? (dt > 0.0 ? p[P_GATE_R + 2u] : p[P_GATE_R + 1u])
                       : (dt > 0.0 ? p[P_GATE_R + 3u] : p[P_GATE_R + 0u]);
    float qs = abs(ds) - (hw - r), qt = abs(dt) - (hh - r);
    float d = length(max(vec2(qs, qt), 0.0)) + min(max(qs, qt), 0.0) - r;
    uint side; float along, len;
    if (qs > qt) { side = ds > 0.0 ? 1u : 3u; along = dt + hh; len = 2.0 * hh; }
    else         { side = dt > 0.0 ? 2u : 0u; along = ds + hw; len = 2.0 * hw; }
    float u = clamp(along / len, 0.0, 1.0), wobble = 0.0;
    for (uint k = 0u; k < 3u; ++k)
        wobble += p[P_WOB_A + side * 3u + k] * sin(float(k + 1u) * kPi * u + p[P_WOB_PH + side * 3u + k]);
    d -= wobble * sin(kPi * u);
    uint nq = uint(p[P_N_Q]);
    for (uint k = 0u; k < nq; ++k) {
        uint q = P_Q + K_Q_STRIDE * k;
        float dq;
        if (abs(p[q + 9u]) > 1.5) {
            vec2 radii = max(vec2(p[q], p[q + 1u]), vec2(1e-4));
            dq = (length((st - vec2(p[q + 0u], p[q + 1u])) / radii) - 1.0) * min(radii.x, radii.y);
        } else dq = sd_quad(st, q) - p[q + 8u];
        d = p[q + 9u] > 0.0 ? min(d, dq) : max(d, -dq);
    }
    if (p[P_ROUGH_AMP] > 0.0) {
        uint seed = uint(p[P_ROUGH_SEED]);
        float period = max(p[P_ROUGH_PERIOD], 1e-3);
        d += p[P_ROUGH_AMP] * (value_noise(st.x / period, seed) + value_noise(st.y / period, seed ^ 0x5bd1e995u) - 1.0);
    }
    return d;
}

float penumbra(float d, float radius) {
    if (radius <= 1e-6) return d < 0.0 ? 1.0 : 0.0;
    float u = clamp(-d / radius, -1.0, 1.0);
    return 0.5 + (u * sqrt(max(1.0 - u * u, 0.0)) + asin(u)) * (1.0 / kPi);
}

vec3 sample_frame(vec3 a, vec3 b, vec3 c, vec3 d, float ax, float ay) {
    return mix(mix(a, b, ax), mix(c, d, ax), ay);
}

uint perf_sdf(vec2 st, out float distance) {
    float film_width = p[P_FILM_W], pitch = p[P_PERF_PITCH], ph = p[P_PERF_PHASE];
    float k = floor((st.x - ph - 0.5 * p[P_PERF_W]) / pitch + 0.5);
    bool top = st.y < 0.5 * film_width;
    uint seed = uint(p[P_PERF_SEED]);
    uint hole = hash_u(uint(int(k) * 2 + (top ? 0 : 1)) * 0x9E3779B9u ^ seed);
    float j0 = hash_f(int(hole & 0xFFFFu), seed ^ 0x1u) - 0.5;
    float j1 = hash_f(int(hole & 0xFFFFu), seed ^ 0x2u) - 0.5;
    float j2 = hash_f(int(hole & 0xFFFFu), seed ^ 0x3u) - 0.5;
    float j3 = hash_f(int(hole & 0xFFFFu), seed ^ 0x4u) - 0.5;
    float j4 = hash_f(int(hole & 0xFFFFu), seed ^ 0x5u) - 0.5;
    float wa = p[P_PERF_W] + 0.016 * j2, hc = p[P_PERF_H] + 0.016 * j3;
    float r = clamp(p[P_PERF_R] + 0.08 * j4, 0.3, 0.6);
    float sc = ph + k * pitch + 0.5 * p[P_PERF_W] + 0.020 * j0;
    float tc = (top ? p[P_PERF_EDGE] + 0.5 * p[P_PERF_H] : film_width - p[P_PERF_EDGE] - 0.5 * p[P_PERF_H]) + 0.024 * j1;
    vec2 q = abs(vec2(st.x - sc, st.y - tc)) - vec2(0.5 * wa - r, 0.5 * hc - r);
    float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
    float rough = 0.001 + 0.003 * hash_f(int(hole & 0xFFFFu), seed ^ 0x6u);
    d += rough * (value_noise(st.x / 0.05, seed ^ 0x77u) + value_noise(st.y / 0.05, seed ^ 0x99u) - 1.0) * 2.0;
    if (hash_f(int(hole & 0xFFFFu), seed ^ 0x7u) < 0.25) {
        float ang = 6.2831853 * hash_f(int(hole & 0xFFFFu), seed ^ 0x8u);
        vec2 pb = vec2(sc, tc) + vec2(cos(ang) * 0.5 * wa, sin(ang) * 0.5 * hc);
        float sig = 0.020 + 0.030 * hash_f(int(hole & 0xFFFFu), seed ^ 0x9u);
        float amp = (hash_f(int(hole & 0xFFFFu), seed ^ 0xAu) < 0.5 ? -1.0 : 1.0) *
                    (0.006 + 0.010 * hash_f(int(hole & 0xFFFFu), seed ^ 0xBu));
        d += amp * exp(-dot(st - pb, st - pb) / (sig * sig));
    }
    distance = d;
    return hole;
}

float erf_as(float x) {
    float t = 1.0 / (1.0 + 0.3275911 * abs(x));
    float y = 1.0 - (((((1.061405429 * t - 1.453152027) * t) + 1.421413741) * t - 0.284496736) * t + 0.254829592) * t * exp(-x * x);
    return x < 0.0 ? -y : y;
}

float film_alpha(vec2 st) {
    float width = p[P_FILM_W], px = p[P_PX];
    float a = p[P_CARRIER] > 1.5 ? 1.0 : clamp(min(st.y, width - st.y) / px + 0.5, 0.0, 1.0);
    if (p[P_PERFORATED] > 0.5) {
        float d; perf_sdf(st, d);
        float sigma = sqrt(0.017 * 0.017 + 0.0961 * px * px);
        a *= 0.5 + 0.5 * erf_as(d / (1.41421356 * sigma));
    }
    return a;
}

#endif
