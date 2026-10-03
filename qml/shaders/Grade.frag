#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

// Stages A and B of the Ajustes pipeline plus the look-only extras - a
// line-for-line mirror of applyGrade() in AdjustMath.cpp (buildAdjustLut,
// ColorMix, applyExtras), which is what actually gets written to disk, so
// preview and file agree. The same shader runs twice in ImageCanvas.qml: once
// for the Filtros look (extras on, `amount` = the Cantidad slider) and once
// for the Ajustes sliders (extras off, amount 1).
//
// Stage A (everything that only looks at one channel at a time: white balance,
// exposure, levels, gamma, blacks/whites, curves, brightness, contrast) is
// precomputed on the CPU into three 256-entry tables and arrives as the 256x1
// `lut` texture - the shader just looks each channel up, so this stage is
// exact rather than approximate. Stage B needs the three channels together
// and is written out here: shadows/highlights, saturation, vibrance, hue
// rotation, negative. Then the extras: gradient map, split toning, vignette,
// grain. Finally the result is blended with the input by `amount`.
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float shadows;
    float highlights;
    float saturation;
    float vibrance;
    float hue;      // -1..1 = -180..180 degrees
    float negative; // 0 or 1
    float amount;   // 0..1, how far from the input toward the graded result
    float gradientOn; // 0 or 1
    float vignette;   // - lightens the corners / + darkens them
    float grain;      // 0..1
    float grainType;  // 0 uniform, 1 gaussian, 2 impulse, 3 laplacian
    float grainMono;  // 0 or 1
    vec2 resolution;  // the image size in pixels (vignette + grain)
    vec3 gradient0;
    vec3 gradient1;
    vec3 gradient2;
    vec3 shadowOffset;
    vec3 highlightOffset;
};

layout(binding = 1) uniform sampler2D source;
layout(binding = 2) uniform sampler2D lut;

const vec3 kLuma = vec3(0.299, 0.587, 0.114);

// Noise / grain. Mirrors noiseHash / noiseSample / applyNoise in AdjustMath.cpp
// line for line: an integer hash of (x, y, channel) drives every draw, so the
// preview and the saved file get exactly the same noise.
uint noiseHash(uvec2 p, uint channel)
{
    uint h = p.x * 0x9E3779B1u + p.y * 0x85EBCA77u + channel * 0xC2B2AE3Du;
    h ^= h >> 16u;
    h *= 0x7FEB352Du;
    h ^= h >> 15u;
    h *= 0x846CA68Bu;
    h ^= h >> 16u;
    return h;
}

// Zero mean, unit variance. type: 0 uniform, 1 gaussian, 3 laplacian.
float noiseSample(int type, uint h)
{
    if (type == 0) {
        float u = float(h & 0xFFFFu) / 65535.0;
        return (2.0 * u - 1.0) * 1.7320508075688772;
    }
    if (type == 3) {
        float v = (float(h & 0xFFFFu) + 0.5) / 65536.0 - 0.5;
        float a = 1.0 - 2.0 * abs(v);
        return (v < 0.0 ? 1.0 : -1.0) * log(a) * 0.7071067811865476;
    }
    float u1 = (float(h & 0xFFFFu) + 1.0) / 65536.0;
    float u2 = float(h >> 16u) / 65536.0;
    return sqrt(-2.0 * log(u1)) * cos(6.283185307179586 * u2);
}

vec3 applyNoise(vec3 c, vec2 px, float amount, float scale, int type, bool mono)
{
    uvec2 p = uvec2(floor(px));
    if (type == 2) {
        float share = amount * 0.12;
        for (int i = 0; i < 3; ++i) {
            float u = (float(noiseHash(p, mono ? 0u : uint(i)) & 0xFFFFu) + 0.5) / 65536.0;
            if (u < share * 0.5)
                c[i] = 0.0;
            else if (u < share)
                c[i] = 1.0;
        }
        return c;
    }
    float l = dot(c, vec3(0.299, 0.587, 0.114));
    float sigma = amount * scale * (0.6 + 0.4 * (1.0 - (2.0 * l - 1.0) * (2.0 * l - 1.0)));
    if (mono)
        return c + vec3(noiseSample(type, noiseHash(p, 0u)) * sigma);
    return c + vec3(noiseSample(type, noiseHash(p, 0u)),
                    noiseSample(type, noiseHash(p, 1u)),
                    noiseSample(type, noiseHash(p, 2u))) * sigma;
}

void main()
{
    // Qt Quick hands every texture over PREMULTIPLIED (rgb already multiplied by the
    // alpha) and expects a premultiplied result. The maths below - like its CPU twin -
    // is on straight colour, so it is undone first and redone at the end. Where the
    // picture is opaque (alpha 1) none of this changes a single value.
    vec4 texel = texture(source, qt_TexCoord0);
    if (amount <= 0.0) {
        fragColor = texel * qt_Opacity;
        return;
    }
    float alpha = texel.a;
    vec3 straight = alpha > 0.0 ? clamp(texel.rgb / alpha, 0.0, 1.0) : vec3(0.0);

    // Stage A: table lookup at texel centers (the lut Image is unfiltered).
    vec3 idx = (straight * 255.0 + 0.5) / 256.0;
    vec3 c = vec3(
        texture(lut, vec2(idx.r, 0.5)).r,
        texture(lut, vec2(idx.g, 0.5)).g,
        texture(lut, vec2(idx.b, 0.5)).b);

    // Stage B
    if (shadows != 0.0 || highlights != 0.0) {
        float l = dot(c, kLuma);
        float ts = clamp(1.0 - 2.0 * l, 0.0, 1.0);
        float th = clamp(2.0 * l - 1.0, 0.0, 1.0);
        c += shadows * 0.40 * (ts * sqrt(ts)) + highlights * 0.40 * (th * sqrt(th));
    }
    if (saturation != 0.0) {
        float l = dot(c, kLuma);
        c = vec3(l) + (c - vec3(l)) * (1.0 + saturation);
    }
    if (vibrance != 0.0) {
        float chroma = clamp(max(max(c.r, c.g), c.b) - min(min(c.r, c.g), c.b), 0.0, 1.0);
        float f = 1.0 + vibrance * (1.0 - chroma);
        float l = dot(c, kLuma);
        c = vec3(l) + (c - vec3(l)) * f;
    }
    if (hue != 0.0) {
        float a = hue * 3.14159265358979;
        float cs = cos(a);
        float sn = sin(a);
        vec3 row0 = vec3(0.213 + cs * 0.787 - sn * 0.213, 0.715 - cs * 0.715 - sn * 0.715, 0.072 - cs * 0.072 + sn * 0.928);
        vec3 row1 = vec3(0.213 - cs * 0.213 + sn * 0.143, 0.715 + cs * 0.285 + sn * 0.140, 0.072 - cs * 0.072 - sn * 0.283);
        vec3 row2 = vec3(0.213 - cs * 0.213 - sn * 0.787, 0.715 - cs * 0.715 + sn * 0.715, 0.072 + cs * 0.928 + sn * 0.072);
        c = vec3(dot(row0, c), dot(row1, c), dot(row2, c));
    }
    c = clamp(c, 0.0, 1.0);
    c = mix(c, vec3(1.0) - c, negative);

    // Look extras (all no-ops for the Ajustes pass, where they are zero/off).
    if (gradientOn > 0.5) {
        float l = dot(c, kLuma);
        vec3 from = l < 0.5 ? gradient0 : gradient1;
        vec3 to = l < 0.5 ? gradient1 : gradient2;
        float t = l < 0.5 ? l * 2.0 : (l - 0.5) * 2.0;
        c = from + (to - from) * t;
    }
    {
        float l = dot(c, kLuma);
        c = clamp(c + shadowOffset * ((1.0 - l) * (1.0 - l)) + highlightOffset * (l * l), 0.0, 1.0);
    }
    if (vignette != 0.0 || grain > 0.0) {
        vec2 px = qt_TexCoord0 * resolution; // pixel center
        if (vignette != 0.0) {
            vec2 mid = resolution * 0.5;
            float maxDist = max(length(mid), 1.0);
            float d = clamp(length(px - mid) / maxDist, 0.0, 1.0);
            float mask = d * d * 0.85;
            c = vignette >= 0.0 ? c * (1.0 - vignette * mask) : c + (vec3(1.0) - c) * (-vignette) * mask;
        }
        if (grain > 0.0) {
            c = applyNoise(c, px, grain, 0.09, int(grainType + 0.5), grainMono > 0.5);
        }
        c = clamp(c, 0.0, 1.0);
    }

    c = mix(straight, c, amount);
    fragColor = vec4(c * alpha, alpha) * qt_Opacity;
}
