#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

// Stage C of the Ajustes pipeline: clarity + sharpen + finishing, mirroring applyDetail()
// in AdjustMath.cpp (the version that runs when the file is saved).
//
// Both effects are computed from the SAME adjusted pixels and added together:
//  - sharpen: 5-tap unsharp mask (center*4 minus its 4 neighbors), scaled by
//    `amount`. texelSize is 1/width, 1/height of the source in pixels, needed
//    to offset the 4 taps by exactly one texel regardless of the item's
//    on-screen (scaled) size.
//  - clarity: local contrast. The pixel is compared with a big-radius blur
//    (a high mip level of this very texture - the source ShaderEffectSource
//    has mipmap: true) and the difference is added back, weighted so the
//    midtones move most and pure blacks/whites don't halo. A negative value
//    softens instead.
//  - vignette (darker or lighter corners) and film grain go on last, after
//    the detail, so the grain is never sharpened.
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float amount;
    float clarity;
    float vignette; // - lightens the corners / + darkens them
    float grain;    // 0..1
    float grainType; // 0 uniform, 1 gaussian, 2 impulse, 3 laplacian
    float grainMono; // 0 or 1
    vec2 texelSize;
    vec2 resolution; // image size in pixels
};

layout(binding = 1) uniform sampler2D source;

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
    // alpha). Neighbourhood maths - sharpen and clarity - is done on exactly those
    // premultiplied values, which is what makes a transparent neighbour weigh
    // nothing (the correct way to filter a picture with alpha), and the CPU twin does
    // the same. Vignette and grain are per-pixel colour changes: those go on straight
    // colour. Where the picture is opaque (alpha 1) none of this changes a value.
    vec4 center = texture(source, qt_TexCoord0);
    bool sharpen = amount > 0.0001;
    bool clarify = abs(clarity) > 0.0001;
    bool finish = vignette != 0.0 || grain > 0.0;
    if (!sharpen && !clarify && !finish) {
        fragColor = center * qt_Opacity;
        return;
    }

    float alpha = center.a;
    vec3 c = center.rgb; // premultiplied
    vec3 result = c;

    if (sharpen) {
        vec3 up = texture(source, qt_TexCoord0 + vec2(0.0, -texelSize.y)).rgb;
        vec3 down = texture(source, qt_TexCoord0 + vec2(0.0, texelSize.y)).rgb;
        vec3 left = texture(source, qt_TexCoord0 + vec2(-texelSize.x, 0.0)).rgb;
        vec3 right = texture(source, qt_TexCoord0 + vec2(texelSize.x, 0.0)).rgb;
        result += amount * (c * 4.0 - (up + down + left + right));
    }

    if (clarify) {
        // Same radius rule as clarityLod() on the CPU: ~2% of the long side,
        // as a power-of-two mip level.
        float longSide = 1.0 / min(texelSize.x, texelSize.y);
        float lod = clamp(floor(log2(max(longSide / 48.0, 1.0)) + 0.5), 2.0, 7.0);
        vec3 blur = textureLod(source, qt_TexCoord0, lod).rgb;
        // the midtone weight looks at the pixel's own (straight) brightness
        vec3 own = alpha > 0.0 ? clamp(c / alpha, 0.0, 1.0) : vec3(0.0);
        float l = dot(own, vec3(0.299, 0.587, 0.114));
        float midtones = 1.0 - (2.0 * l - 1.0) * (2.0 * l - 1.0);
        result += clarity * 1.2 * midtones * (c - blur);
    }

    // Back to straight colour (a premultiplied value cannot exceed its alpha).
    result = alpha > 0.0 ? clamp(result / alpha, 0.0, 1.0) : vec3(0.0);

    if (finish) {
        vec2 px = qt_TexCoord0 * resolution; // pixel center
        if (vignette != 0.0) {
            vec2 mid = resolution * 0.5;
            float maxDist = max(length(mid), 1.0);
            float d = clamp(length(px - mid) / maxDist, 0.0, 1.0);
            float mask = d * d * 0.85;
            result = vignette >= 0.0 ? result * (1.0 - vignette * mask)
                                     : result + (vec3(1.0) - result) * (-vignette) * mask;
        }
        if (grain > 0.0) {
            result = applyNoise(result, px, grain, 0.20, int(grainType + 0.5), grainMono > 0.5);
        }
    }

    fragColor = vec4(clamp(result, 0.0, 1.0) * alpha, alpha) * qt_Opacity;
}
